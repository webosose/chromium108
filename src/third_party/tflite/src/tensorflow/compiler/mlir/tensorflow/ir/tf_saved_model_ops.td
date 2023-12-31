/* Copyright 2019 The TensorFlow Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/

// This is the definition file for the TensorFlow SavedModel dialect.

#ifndef SAVED_MODEL_DIALECT
#define SAVED_MODEL_DIALECT

include "mlir/IR/OpBase.td"
include "mlir/IR/SymbolInterfaces.td"

//===----------------------------------------------------------------------===//
// Dialect definition
//===----------------------------------------------------------------------===//

def TfSavedModel_Dialect : Dialect {
  let name = "tf_saved_model";

  let summary = "Dialect used to represent TensorFlow SavedModel in MLIR.";

  let description = [{
    This dialect's main responsibility is to represent the additional
    information present in the SavedModel's SavedObjectGraph, which
    describes the public interface of this SavedModel.

    Module's using this dialect should have the module attribute
    `tf_saved_model.semantics`.

    This dialect provides an attribute
    `tf_saved_model.exported_names = <array of strings>` which indicates which
    objects in the module are exported, and under what names. The names
    are derived from the object's path in the SavedObjectGraph. For example,
    a Python `tf.Module` with an exported function "foo" will result in an MLIR
    module where the function has an exported name "foo". If the `tf.Module`
    contains a subobject "self.bar" which itself has an exported function
    "baz", then the function will have an exported name "bar.baz".
    If an object in the object graph is reachable via multiple paths
    from the root of the object graph, then this array can have multiple
    entries.
    TODO(silvasean): This design should be reconsidered after
    SavedModel/tf.Module and MLIR linkage semantics are better solidified.
    In particular, it would be great if we could assume a single exported name,
    and use the standardized MLIR `sym_name` attribute to hold it.

    This dialect annotates func ops with two argument attributes
    `tf_saved_model.index_path` and `tf_saved_model.bound_input` in order to
    model the calling API of a function with SavedModel semantics.
    In particular, func's in SavedModel are called with a structured value, and
    return a structured value. A structured value consists of arbitrarily nested
    dicts/lists with tensors as leaves (composite tensors here mostly behave as
    just dicts holding other tensors).
    The arity of the Python-level function is modeled as an outer list.
    Additionally, any variables or constants used by the function are implicitly
    appended to the argument list of the underlying func in a way that is
    transparent to the caller.

    The function arg/result attribute
    `tf_saved_model.index_path = [...some list of strings and integers...]}`
    represents the path which one would use to index into a structured
    value to reach a given tensor. For example, given the structured value
    `[ {"x": leaf0} ]`, the "index path" needed to reach `leaf0` is [0, "x"], as
    it would be if you were indexing in Python into such an object
    (i.e. `obj[0]["x"]`).

    The function arg attribute
    `tf_saved_model.bound_input = @some_symbol_name` references a
    `tf_saved_model.global_tensor` that is bound to that function argument
    and implicitly passed in when the function is invoked from a user-level API.
    TODO(silvasean): Consider replacing tf_saved_model.bound_input arg attrs
    with "get_global @some_global_tensor" in the function body.
  }];

  let cppNamespace = "::mlir::tf_saved_model";

  let emitAccessorPrefix = kEmitAccessorPrefix_Prefixed;
}


//===----------------------------------------------------------------------===//
// Op definitions
//===----------------------------------------------------------------------===//

class TfSavedModel_Op<string mnemonic, list<Trait> traits = []> :
    Op<TfSavedModel_Dialect, mnemonic, traits> {
}

def TfSavedModel_GlobalTensorOp : TfSavedModel_Op<"global_tensor"> {
  let summary = "Represents a global tensor value.";
  let description = [{
    Represents a tensor that is not bound to the lifetime of any particular
    function. Such tensors can be marked as mutable via the `is_mutable`
    attribute.

    These tensors are bound to the arguments of func ops via the
    `tf_saved_model.bound_input` argument attr.

    The `sym_name` represents the symbol table name used for internal IR
    references. The externally visible names, if any, are represented via
    a `tf_saved_model.exported_names` attribute.

    The `value` attribute contains the tensor's value (or initial value, in the
    case it is mutable).

    The `type` attribute contains the tensor's type, which for the case of
    mutable tensors might be more general than just the fixed static shape of
    the `value` attribute. For example, a global tensor might be unranked such
    as `tensor<*xf32>`, or a more complex shape such as `tensor<4x?x27xf32>`.
    The shape of `value` must be compatible with the shape of `type` in the
    sense of `tf.TensorShape` compatibility. And the element types must match.
  }];

  let arguments = (ins
    StrAttr:$sym_name,
    ElementsAttr:$value,
    TypeAttr:$type,
    UnitAttr:$is_mutable
  );

  let hasVerifier = 1;
}

def TfSavedModel_SessionInitializerOp: TfSavedModel_Op<"session_initializer"> {
  let summary = "Initializes TensorFlow session state.";
  let description = [{
    The session initializer op marks one or more functions that must be called
    by an external agent exactly once to initialize TensorFlow session state,
    and this must happen before any other exported functions are called. There
    must be no more than one session initializer op in a saved model.

    The `initializers` represents the initialization functions. The function
    have no output and this function should be only called once.

    This is used, for example, to initialize hash tables stored in resources and
    accessed by resource name (rather than as resource handles or bound inputs
    which is how `global_tensor`s are referenced)
  }];

  let arguments = (ins
    SymbolRefArrayAttr:$initializers
  );


  let hasVerifier = 1;

  let hasCanonicalizer = 1;
}

def TfSavedModel_AssetOp: TfSavedModel_Op<"asset", [Symbol]> {
  let summary = "Represents an asset in saved model.";
  let description = [{
    Represents an asset in the saved model that points to an external file. It
    is a scalar string tensor and it is passed as an argument to the session
    initializer functions.

    The `sym_name` represents the symbol table name used for internal IR
    references.

    The `filename` attribute contains the file path to the asset file and it is
    relative to saved model directory.
  }];

  let arguments = (ins
    StrAttr:$sym_name,
    StrAttr:$filename
  );
}

#endif // SAVED_MODEL_DIALECT
