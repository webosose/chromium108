// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_ACCELERATOR_CONFIGURATION_H_
#define ASH_PUBLIC_CPP_ACCELERATOR_CONFIGURATION_H_

#include <map>
#include <vector>

#include "ash/public/cpp/ash_public_export.h"
#include "ash/public/mojom/accelerator_info.mojom.h"
#include "base/callback.h"
#include "ui/base/accelerators/accelerator.h"

namespace ash {

// Error codes associated with mutating accelerators.
enum class AcceleratorConfigResult {
  kSuccess,            // Success
  kActionLocked,       // Failure, locked actions cannot be modified
  kAcceleratorLocked,  // Failure, locked accelerators cannot be modified
  kConflict,           // Transient failure, conflict with existing accelerator
  kNotFound,           // Failure, accelerator not found
  kDuplicate,          // Failure, adding a duplicate accelerator to the same
                       // action
};

struct ASH_PUBLIC_EXPORT AcceleratorInfo {
  AcceleratorInfo(ash::mojom::AcceleratorType type,
                  ui::Accelerator accelerator,
                  const std::u16string& key_display,
                  bool locked)
      : type(type),
        accelerator(accelerator),
        key_display(key_display),
        locked(locked) {}
  ash::mojom::AcceleratorType type;
  ui::Accelerator accelerator;
  std::u16string key_display;
  // Whether the accelerator can be modified.
  bool locked = true;
  // Accelerators are enabled by default.
  ash::mojom::AcceleratorState state = ash::mojom::AcceleratorState::kEnabled;
};

using AcceleratorActionId = uint32_t;

// The public-facing interface for shortcut providers, this should be
// implemented by sources, e.g. Browser, Ash, that want their shortcuts to be
// exposed to separate clients.
class ASH_PUBLIC_EXPORT AcceleratorConfiguration {
 public:
  using AcceleratorsUpdatedCallback = base::RepeatingCallback<void(
      ash::mojom::AcceleratorSource,
      const std::map<AcceleratorActionId, std::vector<AcceleratorInfo>>&)>;

  explicit AcceleratorConfiguration(ash::mojom::AcceleratorSource source);
  virtual ~AcceleratorConfiguration();

  // Callback will fire immediately once after updating.
  void AddAcceleratorsUpdatedCallback(AcceleratorsUpdatedCallback callback);

  void RemoveAcceleratorsUpdatedCallback(AcceleratorsUpdatedCallback callback);

  // Get all AcceleratorLayoutInfos for an accelerator configuration provider.
  virtual const std::vector<mojom::AcceleratorLayoutInfoPtr>&
  GetAcceleratorLayoutInfos() = 0;

  // Get the accelerators for a single action.
  virtual const std::vector<AcceleratorInfo>& GetConfigForAction(
      AcceleratorActionId action_id) = 0;

  // Whether this source of shortcuts can be modified. If this returns false
  // then any of the Add/Remove/Replace class will DCHECK. The two Restore
  // methods will be no-ops.
  virtual bool IsMutable() const = 0;

  // Add a new user defined accelerator.
  virtual AcceleratorConfigResult AddUserAccelerator(
      AcceleratorActionId action_id,
      const ui::Accelerator& accelerator) = 0;

  // Remove a shortcut. This will delete a user-defined shortcut, or
  // mark a default one disabled.
  virtual AcceleratorConfigResult RemoveAccelerator(
      AcceleratorActionId action_id,
      const ui::Accelerator& accelerator) = 0;

  // Atomic version of Remove then Add.
  virtual AcceleratorConfigResult ReplaceAccelerator(
      AcceleratorActionId action_id,
      const ui::Accelerator& old_acc,
      const ui::Accelerator& new_acc) = 0;

  // Restore the defaults for the given action.
  virtual AcceleratorConfigResult RestoreDefault(
      AcceleratorActionId action_id) = 0;

  // Restore all defaults.
  virtual AcceleratorConfigResult RestoreAllDefaults() = 0;

 protected:
  // Updates the local cache and notifies observers of the updated accelerators.
  void UpdateAccelerators(
      const std::map<AcceleratorActionId, std::vector<AcceleratorInfo>>&
          accelerators);

 private:
  void NotifyAcceleratorsUpdated();

  // The source of the accelerators. Derived classes are responsible for only
  // one source.
  const ash::mojom::AcceleratorSource source_;

  // Container of all invoked callbacks when the accelerators are updated. Call
  // AddAcceleratorsUpdatedCallback or RemoveAcceleratorsUpdatedCallback to
  // add/remove callbacks to the container.
  std::vector<AcceleratorsUpdatedCallback> callbacks_;

  // Keep a cache of the accelerator map, it's possible that adding a new
  // observer is done after initializing the accelerator mapping. This lets
  // new observers to get the immediate cached mapping.
  std::map<AcceleratorActionId, std::vector<AcceleratorInfo>>
      accelerator_mapping_cache_;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_ACCELERATOR_CONFIGURATION_H_
