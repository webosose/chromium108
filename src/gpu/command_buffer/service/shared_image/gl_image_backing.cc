// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image/gl_image_backing.h"

#include "base/trace_event/memory_dump_manager.h"
#include "build/build_config.h"
#include "components/viz/common/resources/resource_format_utils.h"
#include "components/viz/common/resources/resource_sizes.h"
#include "gpu/command_buffer/common/shared_image_trace_utils.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/skia_utils.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/skia/include/core/SkPromiseImageTexture.h"
#include "third_party/skia/include/gpu/GrContextThreadSafeProxy.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_fence.h"
#include "ui/gl/gl_gl_api_implementation.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/scoped_binders.h"
#include "ui/gl/trace_util.h"

#if BUILDFLAG(IS_MAC)
#include "gpu/command_buffer/service/shared_image/iosurface_image_backing_factory.h"
#endif

namespace gpu {

namespace {

using ScopedRestoreTexture = GLTextureImageBackingHelper::ScopedRestoreTexture;

using InitializeGLTextureParams =
    GLTextureImageBackingHelper::InitializeGLTextureParams;

}  // namespace

///////////////////////////////////////////////////////////////////////////////
// GLTextureGLCommonRepresentation

// Representation of a GLTextureImageBacking as a GL Texture.
GLTextureGLCommonRepresentation::GLTextureGLCommonRepresentation(
    SharedImageManager* manager,
    SharedImageBacking* backing,
    GLTextureImageRepresentationClient* client,
    MemoryTypeTracker* tracker,
    gles2::Texture* texture)
    : GLTextureImageRepresentation(manager, backing, tracker),
      client_(client),
      texture_(texture) {}

GLTextureGLCommonRepresentation::~GLTextureGLCommonRepresentation() {
  texture_ = nullptr;
  if (client_)
    client_->GLTextureImageRepresentationRelease(has_context());
}

gles2::Texture* GLTextureGLCommonRepresentation::GetTexture(int plane_index) {
  DCHECK_EQ(plane_index, 0);
  return texture_;
}

bool GLTextureGLCommonRepresentation::BeginAccess(GLenum mode) {
  DCHECK(mode_ == 0);
  mode_ = mode;
  bool readonly = mode_ != GL_SHARED_IMAGE_ACCESS_MODE_READWRITE_CHROMIUM;
  if (client_ && mode != GL_SHARED_IMAGE_ACCESS_MODE_OVERLAY_CHROMIUM)
    return client_->GLTextureImageRepresentationBeginAccess(readonly);
  return true;
}

void GLTextureGLCommonRepresentation::EndAccess() {
  DCHECK(mode_ != 0);
  GLenum current_mode = mode_;
  mode_ = 0;
  if (client_)
    return client_->GLTextureImageRepresentationEndAccess(
        current_mode != GL_SHARED_IMAGE_ACCESS_MODE_READWRITE_CHROMIUM);
}

///////////////////////////////////////////////////////////////////////////////
// GLTexturePassthroughGLCommonRepresentation

GLTexturePassthroughGLCommonRepresentation::
    GLTexturePassthroughGLCommonRepresentation(
        SharedImageManager* manager,
        SharedImageBacking* backing,
        GLTextureImageRepresentationClient* client,
        MemoryTypeTracker* tracker,
        scoped_refptr<gles2::TexturePassthrough> texture_passthrough)
    : GLTexturePassthroughImageRepresentation(manager, backing, tracker),
      client_(client),
      texture_passthrough_(std::move(texture_passthrough)) {
  // TODO(https://crbug.com/1172769): Remove this CHECK.
  CHECK(texture_passthrough_);
}

GLTexturePassthroughGLCommonRepresentation::
    ~GLTexturePassthroughGLCommonRepresentation() {
  texture_passthrough_.reset();
  if (client_)
    client_->GLTextureImageRepresentationRelease(has_context());
}

const scoped_refptr<gles2::TexturePassthrough>&
GLTexturePassthroughGLCommonRepresentation::GetTexturePassthrough(
    int plane_index) {
  DCHECK_EQ(plane_index, 0);
  return texture_passthrough_;
}

bool GLTexturePassthroughGLCommonRepresentation::BeginAccess(GLenum mode) {
  DCHECK(mode_ == 0);
  mode_ = mode;
  bool readonly = mode_ != GL_SHARED_IMAGE_ACCESS_MODE_READWRITE_CHROMIUM;
  if (client_ && mode != GL_SHARED_IMAGE_ACCESS_MODE_OVERLAY_CHROMIUM)
    return client_->GLTextureImageRepresentationBeginAccess(readonly);
  return true;
}

void GLTexturePassthroughGLCommonRepresentation::EndAccess() {
  DCHECK(mode_ != 0);
  GLenum current_mode = mode_;
  mode_ = 0;
  if (client_)
    return client_->GLTextureImageRepresentationEndAccess(
        current_mode != GL_SHARED_IMAGE_ACCESS_MODE_READWRITE_CHROMIUM);
}

///////////////////////////////////////////////////////////////////////////////
// SkiaGLCommonRepresentation

SkiaGLCommonRepresentation::SkiaGLCommonRepresentation(
    SharedImageManager* manager,
    SharedImageBacking* backing,
    GLTextureImageRepresentationClient* client,
    scoped_refptr<SharedContextState> context_state,
    sk_sp<SkPromiseImageTexture> promise_texture,
    MemoryTypeTracker* tracker)
    : SkiaImageRepresentation(manager, backing, tracker),
      client_(client),
      context_state_(std::move(context_state)),
      promise_texture_(promise_texture) {
  DCHECK(promise_texture_);
#if DCHECK_IS_ON()
  if (context_state_->GrContextIsGL())
    context_ = gl::GLContext::GetCurrent();
#endif
}

SkiaGLCommonRepresentation::~SkiaGLCommonRepresentation() {
  if (write_surface_) {
    DLOG(ERROR) << "SkiaImageRepresentation was destroyed while still "
                << "open for write access.";
  }
  promise_texture_.reset();
  if (client_) {
    DCHECK(context_state_->GrContextIsGL());
    client_->GLTextureImageRepresentationRelease(has_context());
  }
}

sk_sp<SkSurface> SkiaGLCommonRepresentation::BeginWriteAccess(
    int final_msaa_count,
    const SkSurfaceProps& surface_props,
    std::vector<GrBackendSemaphore>* begin_semaphores,
    std::vector<GrBackendSemaphore>* end_semaphores,
    std::unique_ptr<GrBackendSurfaceMutableState>* end_state) {
  CheckContext();
  if (client_) {
    DCHECK(context_state_->GrContextIsGL());
    if (!client_->GLTextureImageRepresentationBeginAccess(
            /*readonly=*/false)) {
      return nullptr;
    }
  }

  if (write_surface_)
    return nullptr;

  if (!promise_texture_)
    return nullptr;

  SkColorType sk_color_type = viz::ResourceFormatToClosestSkColorType(
      /*gpu_compositing=*/true, format());
  // Gray is not a renderable single channel format, but alpha is.
  if (sk_color_type == kGray_8_SkColorType)
    sk_color_type = kAlpha_8_SkColorType;
  auto surface = SkSurface::MakeFromBackendTexture(
      context_state_->gr_context(), promise_texture_->backendTexture(),
      surface_origin(), final_msaa_count, sk_color_type,
      backing()->color_space().GetAsFullRangeRGB().ToSkColorSpace(),
      &surface_props);
  write_surface_ = surface.get();
  return surface;
}

sk_sp<SkPromiseImageTexture> SkiaGLCommonRepresentation::BeginWriteAccess(
    std::vector<GrBackendSemaphore>* begin_semaphores,
    std::vector<GrBackendSemaphore>* end_semaphores,
    std::unique_ptr<GrBackendSurfaceMutableState>* end_state) {
  CheckContext();
  if (client_) {
    DCHECK(context_state_->GrContextIsGL());
    if (!client_->GLTextureImageRepresentationBeginAccess(
            /*readonly=*/false)) {
      return nullptr;
    }
  }
  return promise_texture_;
}

void SkiaGLCommonRepresentation::EndWriteAccess(sk_sp<SkSurface> surface) {
  if (surface) {
    DCHECK_EQ(surface.get(), write_surface_);
    DCHECK(surface->unique());
    CheckContext();
    // TODO(ericrk): Keep the surface around for re-use.
    write_surface_ = nullptr;
  }

  if (client_)
    client_->GLTextureImageRepresentationEndAccess(false /* readonly */);
}

sk_sp<SkPromiseImageTexture> SkiaGLCommonRepresentation::BeginReadAccess(
    std::vector<GrBackendSemaphore>* begin_semaphores,
    std::vector<GrBackendSemaphore>* end_semaphores,
    std::unique_ptr<GrBackendSurfaceMutableState>* end_state) {
  CheckContext();
  if (client_) {
    DCHECK(context_state_->GrContextIsGL());
    if (!client_->GLTextureImageRepresentationBeginAccess(
            /*readonly=*/true)) {
      return nullptr;
    }
  }
  return promise_texture_;
}

void SkiaGLCommonRepresentation::EndReadAccess() {
  if (client_)
    client_->GLTextureImageRepresentationEndAccess(true /* readonly */);
}

bool SkiaGLCommonRepresentation::SupportsMultipleConcurrentReadAccess() {
  return true;
}

void SkiaGLCommonRepresentation::CheckContext() {
#if DCHECK_IS_ON()
  if (!context_state_->context_lost() && context_)
    DCHECK(gl::GLContext::GetCurrent() == context_);
#endif
}

///////////////////////////////////////////////////////////////////////////////
// OverlayGLImageRepresentation

OverlayGLImageRepresentation::OverlayGLImageRepresentation(
    SharedImageManager* manager,
    SharedImageBacking* backing,
    MemoryTypeTracker* tracker,
    scoped_refptr<gl::GLImage> gl_image)
    : OverlayImageRepresentation(manager, backing, tracker),
      gl_image_(gl_image) {}

OverlayGLImageRepresentation::~OverlayGLImageRepresentation() = default;

bool OverlayGLImageRepresentation::BeginReadAccess(
    gfx::GpuFenceHandle& acquire_fence) {
  auto* gl_backing = static_cast<GLImageBacking*>(backing());
  std::unique_ptr<gfx::GpuFence> fence = gl_backing->GetLastWriteGpuFence();
  if (fence)
    acquire_fence = fence->GetGpuFenceHandle().Clone();
  return true;
}

void OverlayGLImageRepresentation::EndReadAccess(
    gfx::GpuFenceHandle release_fence) {
  auto* gl_backing = static_cast<GLImageBacking*>(backing());
  gl_backing->SetReleaseFence(std::move(release_fence));
}

gl::GLImage* OverlayGLImageRepresentation::GetGLImage() {
  return gl_image_.get();
}

///////////////////////////////////////////////////////////////////////////////
// GLImageBacking

// static
std::unique_ptr<GLImageBacking> GLImageBacking::CreateFromGLTexture(
    scoped_refptr<gl::GLImage> image,
    const Mailbox& mailbox,
    viz::ResourceFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    uint32_t usage,
    GLenum texture_target,
    scoped_refptr<gles2::TexturePassthrough> wrapped_gl_texture) {
  DCHECK(!!wrapped_gl_texture);

  // We don't expect the backing to allocate a new
  // texture but it does need to know the texture target so we supply that
  // one param.
  InitializeGLTextureParams params;
  params.target = texture_target;

  auto si_format = viz::SharedImageFormat::SinglePlane(format);
  auto shared_image = std::make_unique<GLImageBacking>(
      std::move(image), mailbox, si_format, size, color_space, surface_origin,
      alpha_type, usage, params, true);

  shared_image->passthrough_texture_ = std::move(wrapped_gl_texture);
  shared_image->gl_texture_retained_for_legacy_mailbox_ = true;
  shared_image->gl_texture_retain_count_ = 1;
  shared_image->image_bind_or_copy_needed_ = false;

  return shared_image;
}

GLImageBacking::GLImageBacking(scoped_refptr<gl::GLImage> image,
                               const Mailbox& mailbox,
                               viz::SharedImageFormat format,
                               const gfx::Size& size,
                               const gfx::ColorSpace& color_space,
                               GrSurfaceOrigin surface_origin,
                               SkAlphaType alpha_type,
                               uint32_t usage,
                               const InitializeGLTextureParams& params,
                               bool is_passthrough)
    : SharedImageBacking(
          mailbox,
          format,
          size,
          color_space,
          surface_origin,
          alpha_type,
          usage,
          viz::ResourceSizes::UncheckedSizeInBytes<size_t>(size, format),
          false /* is_thread_safe */),
      image_(image),
      gl_params_(params),
      is_passthrough_(is_passthrough),
      cleared_rect_(params.is_cleared ? gfx::Rect(size) : gfx::Rect()),
      weak_factory_(this) {
  DCHECK(image_);
#if BUILDFLAG(IS_MAC)
  // NOTE: Mac currently retains GLTexture and reuses it. Not sure if this is
  // best approach as it can lead to issues with context losses.
  if (!gl_texture_retained_for_legacy_mailbox_) {
    RetainGLTexture();
    gl_texture_retained_for_legacy_mailbox_ = true;
  }
#endif
}

GLImageBacking::~GLImageBacking() {
  if (gl_texture_retained_for_legacy_mailbox_)
    ReleaseGLTexture(have_context());
  DCHECK_EQ(gl_texture_retain_count_, 0u);
}

void GLImageBacking::RetainGLTexture() {
  gl_texture_retain_count_ += 1;
  if (gl_texture_retain_count_ > 1)
    return;

  // Allocate the GL texture.
  GLTextureImageBackingHelper::MakeTextureAndSetParameters(
      gl_params_.target, 0 /* service_id */,
      gl_params_.framebuffer_attachment_angle,
      is_passthrough_ ? &passthrough_texture_ : nullptr,
      is_passthrough_ ? nullptr : &texture_);

  // Set the GLImage to be initially unbound from the GL texture.
  image_bind_or_copy_needed_ = true;
  if (is_passthrough_) {
    passthrough_texture_->SetEstimatedSize(
        viz::ResourceSizes::UncheckedSizeInBytes<size_t>(size(), format()));
    passthrough_texture_->SetLevelImage(gl_params_.target, 0, image_.get());
    passthrough_texture_->set_is_bind_pending(true);
  } else {
    texture_->SetLevelInfo(gl_params_.target, 0, gl_params_.internal_format,
                           size().width(), size().height(), 1, 0,
                           gl_params_.format, gl_params_.type, cleared_rect_);
    texture_->SetLevelImage(gl_params_.target, 0, image_.get(),
                            gles2::Texture::UNBOUND);
    texture_->SetImmutable(true, false /* has_immutable_storage */);
  }
}

void GLImageBacking::ReleaseGLTexture(bool have_context) {
  DCHECK_GT(gl_texture_retain_count_, 0u);
  gl_texture_retain_count_ -= 1;
  if (gl_texture_retain_count_ > 0)
    return;

  // If the cached promise texture is referencing the GL texture, then it needs
  // to be deleted, too.
  if (cached_promise_texture_) {
    if (cached_promise_texture_->backendTexture().backend() ==
        GrBackendApi::kOpenGL) {
      cached_promise_texture_.reset();
    }
  }

  if (IsPassthrough()) {
    if (passthrough_texture_) {
      if (have_context) {
        if (!passthrough_texture_->is_bind_pending()) {
          const GLenum target = GetGLTarget();
          gl::ScopedTextureBinder binder(target,
                                         passthrough_texture_->service_id());
          image_->ReleaseTexImage(target);
        }
      } else {
        passthrough_texture_->MarkContextLost();
      }
      passthrough_texture_.reset();
    }
  } else {
    if (texture_) {
      cleared_rect_ = texture_->GetLevelClearedRect(texture_->target(), 0);
      texture_->RemoveLightweightRef(have_context);
      texture_ = nullptr;
    }
  }
}

GLenum GLImageBacking::GetGLTarget() const {
  return gl_params_.target;
}

GLuint GLImageBacking::GetGLServiceId() const {
  if (texture_)
    return texture_->service_id();
  if (passthrough_texture_)
    return passthrough_texture_->service_id();
  return 0;
}

std::unique_ptr<gfx::GpuFence> GLImageBacking::GetLastWriteGpuFence() {
  return last_write_gl_fence_ ? last_write_gl_fence_->GetGpuFence() : nullptr;
}

void GLImageBacking::SetReleaseFence(gfx::GpuFenceHandle release_fence) {
  release_fence_ = std::move(release_fence);
}

scoped_refptr<gfx::NativePixmap> GLImageBacking::GetNativePixmap() {
  return image_->GetNativePixmap();
}

void GLImageBacking::OnMemoryDump(
    const std::string& dump_name,
    base::trace_event::MemoryAllocatorDumpGuid client_guid,
    base::trace_event::ProcessMemoryDump* pmd,
    uint64_t client_tracing_id) {
  SharedImageBacking::OnMemoryDump(dump_name, client_guid, pmd,
                                   client_tracing_id);

  // Add a |service_guid| which expresses shared ownership between the
  // various GPU dumps.
  if (auto service_id = GetGLServiceId()) {
    auto service_guid = gl::GetGLTextureServiceGUIDForTracing(GetGLServiceId());
    pmd->CreateSharedGlobalAllocatorDump(service_guid);
    pmd->AddOwnershipEdge(client_guid, service_guid, kOwningEdgeImportance);
  }
  image_->OnMemoryDump(pmd, client_tracing_id, dump_name);
}

SharedImageBackingType GLImageBacking::GetType() const {
  return SharedImageBackingType::kGLImage;
}

gfx::Rect GLImageBacking::ClearedRect() const {
  if (texture_)
    return texture_->GetLevelClearedRect(texture_->target(), 0);
  return cleared_rect_;
}

void GLImageBacking::SetClearedRect(const gfx::Rect& cleared_rect) {
  if (texture_)
    texture_->SetLevelClearedRect(texture_->target(), 0, cleared_rect);
  else
    cleared_rect_ = cleared_rect;
}

std::unique_ptr<GLTextureImageRepresentation> GLImageBacking::ProduceGLTexture(
    SharedImageManager* manager,
    MemoryTypeTracker* tracker) {
  // The corresponding release will be done when the returned representation is
  // destroyed, in GLTextureImageRepresentationRelease.
  RetainGLTexture();
  DCHECK(texture_);
  return std::make_unique<GLTextureGLCommonRepresentation>(manager, this, this,
                                                           tracker, texture_);
}
std::unique_ptr<GLTexturePassthroughImageRepresentation>
GLImageBacking::ProduceGLTexturePassthrough(SharedImageManager* manager,
                                            MemoryTypeTracker* tracker) {
  // The corresponding release will be done when the returned representation is
  // destroyed, in GLTextureImageRepresentationRelease.
  RetainGLTexture();
  DCHECK(passthrough_texture_);
  return std::make_unique<GLTexturePassthroughGLCommonRepresentation>(
      manager, this, this, tracker, passthrough_texture_);
}

std::unique_ptr<OverlayImageRepresentation> GLImageBacking::ProduceOverlay(
    SharedImageManager* manager,
    MemoryTypeTracker* tracker) {
#if BUILDFLAG(IS_MAC) || defined(USE_OZONE) || BUILDFLAG(IS_WIN)
  return std::make_unique<OverlayGLImageRepresentation>(manager, this, tracker,
                                                        image_);
#else   // !(BUILDFLAG(IS_MAC) || defined(USE_OZONE) || BUILDFLAG(IS_WIN))
  return SharedImageBacking::ProduceOverlay(manager, tracker);
#endif  // BUILDFLAG(IS_MAC) || defined(USE_OZONE) || BUILDFLAG(IS_WIN)
}

std::unique_ptr<DawnImageRepresentation> GLImageBacking::ProduceDawn(
    SharedImageManager* manager,
    MemoryTypeTracker* tracker,
    WGPUDevice device,
    WGPUBackendType backend_type) {
#if BUILDFLAG(IS_MAC)
  auto result = IOSurfaceImageBackingFactory::ProduceDawn(
      manager, this, tracker, device, image_);
  if (result)
    return result;
#endif  // BUILDFLAG(IS_MAC)
  if (!factory()) {
    DLOG(ERROR) << "No SharedImageFactory to create a dawn representation.";
    return nullptr;
  }

  return GLTextureImageBackingHelper::ProduceDawnCommon(
      factory(), manager, tracker, device, backend_type, this, IsPassthrough());
}

std::unique_ptr<SkiaImageRepresentation> GLImageBacking::ProduceSkia(
    SharedImageManager* manager,
    MemoryTypeTracker* tracker,
    scoped_refptr<SharedContextState> context_state) {
  GLTextureImageRepresentationClient* gl_client = nullptr;
  if (context_state->GrContextIsGL()) {
    // The corresponding release will be done when the returned representation
    // is destroyed, in GLTextureImageRepresentationRelease.
    RetainGLTexture();
    gl_client = this;
  }

  if (!cached_promise_texture_) {
    if (context_state->GrContextIsMetal()) {
#if BUILDFLAG(IS_MAC)
      cached_promise_texture_ =
          IOSurfaceImageBackingFactory::ProduceSkiaPromiseTextureMetal(
              this, context_state, image_);
      DCHECK(cached_promise_texture_);
#endif
    } else {
      GrBackendTexture backend_texture;
      GetGrBackendTexture(context_state->feature_info(), GetGLTarget(), size(),
                          GetGLServiceId(), format().resource_format(),
                          context_state->gr_context()->threadSafeProxy(),
                          &backend_texture);
      cached_promise_texture_ = SkPromiseImageTexture::Make(backend_texture);
    }
  }
  return std::make_unique<SkiaGLCommonRepresentation>(
      manager, this, gl_client, std::move(context_state),
      cached_promise_texture_, tracker);
}

MemoryGLImageRepresentation::MemoryGLImageRepresentation(
    SharedImageManager* manager,
    SharedImageBacking* backing,
    MemoryTypeTracker* tracker,
    scoped_refptr<gl::GLImageMemory> image_memory)
    : MemoryImageRepresentation(manager, backing, tracker),
      image_memory_(std::move(image_memory)) {}

MemoryGLImageRepresentation::~MemoryGLImageRepresentation() = default;

SkPixmap MemoryGLImageRepresentation::BeginReadAccess() {
  return SkPixmap(backing()->AsSkImageInfo(), image_memory_->memory(),
                  image_memory_->stride());
}

std::unique_ptr<MemoryImageRepresentation> GLImageBacking::ProduceMemory(
    SharedImageManager* manager,
    MemoryTypeTracker* tracker) {
  gl::GLImageMemory* image_memory =
      gl::GLImageMemory::FromGLImage(image_.get());
  if (!image_memory)
    return nullptr;

  return std::make_unique<MemoryGLImageRepresentation>(
      manager, this, tracker, base::WrapRefCounted(image_memory));
}

void GLImageBacking::Update(std::unique_ptr<gfx::GpuFence> in_fence) {
  if (in_fence) {
    // TODO(dcastagna): Don't wait for the fence if the SharedImage is going
    // to be scanned out as an HW overlay. Currently we don't know that at
    // this point and we always bind the image, therefore we need to wait for
    // the fence.
    std::unique_ptr<gl::GLFence> egl_fence =
        gl::GLFence::CreateFromGpuFence(*in_fence.get());
    egl_fence->ServerWait();
  }
  image_bind_or_copy_needed_ = true;
}

bool GLImageBacking::GLTextureImageRepresentationBeginAccess(bool readonly) {
#if BUILDFLAG(IS_MAC)
  DCHECK(!ongoing_write_access_);
  if (readonly) {
    num_ongoing_read_accesses_++;
  } else {
    if (!(usage() & SHARED_IMAGE_USAGE_CONCURRENT_READ_WRITE)) {
      DCHECK(num_ongoing_read_accesses_ == 0);
    }
    ongoing_write_access_ = true;
  }
#endif

  if (!release_fence_.is_null()) {
    auto fence = gfx::GpuFence(std::move(release_fence_));
    if (gl::GLFence::IsGpuFenceSupported()) {
      gl::GLFence::CreateFromGpuFence(std::move(fence))->ServerWait();
    } else {
      fence.Wait();
    }
  }
  return BindOrCopyImageIfNeeded();
}

void GLImageBacking::GLTextureImageRepresentationEndAccess(bool readonly) {
#if BUILDFLAG(IS_MAC)
  if (readonly) {
    DCHECK(num_ongoing_read_accesses_ > 0);
    if (!(usage() & SHARED_IMAGE_USAGE_CONCURRENT_READ_WRITE)) {
      DCHECK(!ongoing_write_access_);
    }
    num_ongoing_read_accesses_--;
  } else {
    DCHECK(ongoing_write_access_);
    if (!(usage() & SHARED_IMAGE_USAGE_CONCURRENT_READ_WRITE)) {
      DCHECK(num_ongoing_read_accesses_ == 0);
    }
    ongoing_write_access_ = false;
  }

  // If this image could potentially be shared with Metal via WebGPU, then flush
  // the GL context to ensure Metal will see it.
  if (usage() & SHARED_IMAGE_USAGE_WEBGPU) {
    gl::GLApi* api = gl::g_current_gl_context;
    api->glFlushFn();
  }

  // When SwANGLE is used as the GL implementation, it holds an internal
  // texture. We have to call ReleaseTexImage here to trigger a copy from that
  // internal texture to the IOSurface (the next Bind() will then trigger an
  // IOSurface->internal texture copy). We do this only when there are no
  // ongoing reads in order to ensure that it does not result in the GLES2
  // decoders needing to perform on-demand binding (rather, the binding will be
  // performed at the next BeginAccess()). Note that it is not sufficient to
  // release the image only at the end of a write: the CPU can write directly to
  // the IOSurface when the GPU is not accessing the internal texture (in the
  // case of zero-copy raster), and any such IOSurface-side modifications need
  // to be copied to the internal texture via a Bind() when the GPU starts a
  // subsequent read. Note also that this logic assumes that writes are
  // serialized with respect to reads (so that the end of a write always
  // triggers a release and copy). By design, GLImageBackingFactory enforces
  // this property for this use case.
  bool needs_sync_for_swangle =
      (gl::GetANGLEImplementation() == gl::ANGLEImplementation::kSwiftShader &&
       (num_ongoing_read_accesses_ == 0));

  // Similarly, when ANGLE's metal backend is used, we have to signal a call to
  // waitUntilScheduled() using the same method on EndAccess to ensure IOSurface
  // synchronization. In this case, it is sufficient to release the image at the
  // end of a write. As above, GLImageBackingFactory enforces serialization of
  // reads and writes for this use case.
  // TODO(https://anglebug.com/7626): Enable on Metal only when
  // CPU_READ or SCANOUT is specified. When doing so, adjust the conditions for
  // disallowing concurrent read/write in GLImageBackingFactory as suitable.
  bool needs_sync_for_metal =
      (gl::GetANGLEImplementation() == gl::ANGLEImplementation::kMetal &&
       !readonly);

  bool needs_synchronization =
      IsPassthrough() && (needs_sync_for_swangle || needs_sync_for_metal);
  if (needs_synchronization &&
      image_->ShouldBindOrCopy() == gl::GLImage::BIND) {
    const GLenum target = GetGLTarget();
    gl::ScopedTextureBinder binder(target, passthrough_texture_->service_id());
    if (!passthrough_texture_->is_bind_pending()) {
      image_->ReleaseTexImage(target);
      image_bind_or_copy_needed_ = true;
      passthrough_texture_->set_is_bind_pending(true);
    }
  }
#else

  // If the image will be used for an overlay, we insert a fence that can be
  // used by OutputPresenter to synchronize image writes with presentation.
  if (!readonly && usage() & SHARED_IMAGE_USAGE_SCANOUT &&
      gl::GLFence::IsGpuFenceSupported()) {
    // If the image will be used for delegated compositing, no need to put
    // fences at this moment as there are many raster tasks in the CPU gl
    // context that end up creating a big number of fences, which may have some
    // performance overhead depending on the gpu. Instead, when these images
    // will be scheduled as overlays, a single fence will be created.
    // TODO(crbug.com/1254033): this block of code shall be removed after cc is
    // able to set a single (duplicated) fence for bunch of tiles instead of
    // having the SI framework creating fences for each single message when
    // write access ends.
    if (usage() & SHARED_IMAGE_USAGE_RASTER_DELEGATED_COMPOSITING)
      return;

    last_write_gl_fence_ = gl::GLFence::CreateForGpuFence();
    DCHECK(last_write_gl_fence_);
  }
#endif
}

void GLImageBacking::GLTextureImageRepresentationRelease(bool has_context) {
  ReleaseGLTexture(has_context);
}

bool GLImageBacking::BindOrCopyImageIfNeeded() {
  // This is called by code that has retained the GL texture.
  DCHECK(texture_ || passthrough_texture_);
  if (!image_bind_or_copy_needed_)
    return true;

  const GLenum target = GetGLTarget();
  gl::GLApi* api = gl::g_current_gl_context;
  ScopedRestoreTexture scoped_restore(api, target);
  api->glBindTextureFn(target, GetGLServiceId());

  // Un-bind the GLImage from the texture if it is currently bound.
  if (image_->ShouldBindOrCopy() == gl::GLImage::BIND) {
    bool is_bound = false;
    if (IsPassthrough()) {
      is_bound = !passthrough_texture_->is_bind_pending();
    } else {
      gles2::Texture::ImageState old_state = gles2::Texture::UNBOUND;
      texture_->GetLevelImage(target, 0, &old_state);
      is_bound = old_state == gles2::Texture::BOUND;
    }
    if (is_bound)
      image_->ReleaseTexImage(target);
  }

  // Bind or copy the GLImage to the texture.
  gles2::Texture::ImageState new_state = gles2::Texture::UNBOUND;
  if (image_->ShouldBindOrCopy() == gl::GLImage::BIND) {
    if (!image_->BindTexImage(target)) {
      LOG(ERROR) << "Failed to bind GLImage to target";
      return false;
    }
    new_state = gles2::Texture::BOUND;
  } else {
    ScopedUnpackState scoped_unpack_state(
        /*uploading_data=*/true);
    if (!image_->CopyTexImage(target)) {
      LOG(ERROR) << "Failed to copy GLImage to target";
      return false;
    }
    new_state = gles2::Texture::COPIED;
  }
  DCHECK(new_state == gles2::Texture::BOUND ||
         new_state == gles2::Texture::COPIED);
  if (IsPassthrough()) {
    passthrough_texture_->set_is_bind_pending(false);
  } else {
    texture_->SetLevelImage(target, 0, image_.get(), new_state);
  }

  image_bind_or_copy_needed_ = false;
  return true;
}

void GLImageBacking::InitializePixels(GLenum format,
                                      GLenum type,
                                      const uint8_t* data) {
  DCHECK_EQ(image_->ShouldBindOrCopy(), gl::GLImage::BIND);
#if BUILDFLAG(IS_MAC)
  if (IOSurfaceImageBackingFactory::InitializePixels(this, image_, data))
    return;
#else
  RetainGLTexture();
  BindOrCopyImageIfNeeded();

  const GLenum target = GetGLTarget();
  gl::GLApi* api = gl::g_current_gl_context;
  ScopedRestoreTexture scoped_restore(api, target);
  api->glBindTextureFn(target, GetGLServiceId());
  ScopedUnpackState scoped_unpack_state(
      /*uploading_data=*/true);
  api->glTexSubImage2DFn(target, 0, 0, 0, size().width(), size().height(),
                         format, type, data);
  ReleaseGLTexture(true /* have_context */);
#endif
}

}  // namespace gpu
