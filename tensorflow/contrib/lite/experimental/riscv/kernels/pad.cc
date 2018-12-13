/* Copyright 2017 The TensorFlow Authors. All Rights Reserved.

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
#include <string.h>
#include <vector>
#include "tensorflow/contrib/lite/c/builtin_op_data.h"
#include "tensorflow/contrib/lite/c/c_api_internal.h"
#include "tensorflow/contrib/lite/experimental/riscv/kernels/reference/reference_ops_float.h"
#include "tensorflow/contrib/lite/kernels/internal/tensor.h"
#include "tensorflow/contrib/lite/kernels/kernel_util.h"
#include "tensorflow/contrib/lite/kernels/op_macros.h"

namespace tflite {
namespace ops {
namespace riscv {
namespace pad {

// This file has two implementations of Pad.
enum KernelType {
  kReference,
  kOptimized,
};

struct PadContext {
  PadContext(TfLiteContext* context, TfLiteNode* node) {
    input = GetInput(context, node, 0);
    paddings = GetInput(context, node, 1);
    if (NumInputs(node) == 3) {
      constant_values = GetOptionalInputTensor(context, node, 2);
    } else {
      constant_values = nullptr;
    }
    output = GetOutput(context, node, 0);
    dims = NumDimensions(input);
  }
  const TfLiteTensor* constant_values;
  const TfLiteTensor* input;
  const TfLiteTensor* paddings;
  TfLiteTensor* output;
  int dims;
};

// Resizes output array based on the input size and padding size. This function
// is callable from both Prepare() and Eval() as long as the caller ensures the
// paddings data is present.
TfLiteStatus ResizeOutputTensor(TfLiteContext* context,
                                PadContext* op_context) {
  // Ensures the paddings array is dims x 2.
  TF_LITE_ENSURE_EQ(context, SizeOfDimension(op_context->paddings, 0),
                    op_context->dims);
  TF_LITE_ENSURE_EQ(context, SizeOfDimension(op_context->paddings, 1), 2);

  // Determines the size of the output tensor.
  TfLiteIntArray* input_size = op_context->input->dims;
  TfLiteIntArray* output_size = TfLiteIntArrayCopy(input_size);
  const int32* paddings_data = GetTensorData<int32>(op_context->paddings);

  for (int idx = 0; idx < op_context->dims; ++idx) {
    int before_padding = *paddings_data++;
    int after_padding = *paddings_data++;

    TF_LITE_ENSURE_MSG(context, (before_padding >= 0 && after_padding >= 0),
                       "Pad value has to be greater than equal to 0.");

    output_size->data[idx] =
        (input_size->data[idx] + before_padding + after_padding);
  }

  return context->ResizeTensor(context, op_context->output, output_size);
}

TfLiteStatus Prepare(TfLiteContext* context, TfLiteNode* node) {
  TF_LITE_ENSURE(context, NumInputs(node) == 2 || NumInputs(node) == 3);
  TF_LITE_ENSURE_EQ(context, NumOutputs(node), 1);

  PadContext op_context(context, node);
  TF_LITE_ENSURE_EQ(context, op_context.input->type, op_context.output->type);
  if (op_context.constant_values != nullptr) {
    TF_LITE_ENSURE_EQ(context, op_context.input->type,
                      op_context.constant_values->type);
  }

  // TODO(nupurgarg): Current implementations rely on the inputs being <= 4D.
  TF_LITE_ENSURE(context, op_context.dims <= 4);

  // Exit early if paddings is a non-const tensor. Set output tensor to
  // dynamic so output size can be determined in Eval.
  if (!IsConstantTensor(op_context.paddings)) {
    SetTensorToDynamic(op_context.output);
    return kTfLiteOk;
  }
  return ResizeOutputTensor(context, &op_context);
}

template <KernelType kernel_type>
TfLiteStatus Eval(TfLiteContext* context, TfLiteNode* node) {
  PadContext op_context(context, node);

  if (op_context.constant_values != nullptr) {
    // Ensure that constant_values is a scalar.
    TF_LITE_ENSURE_EQ(context, NumElements(op_context.constant_values), 1);
  }

  // Resize the output tensor if the output tensor is dynamic.
  if (IsDynamicTensor(op_context.output)) {
    TF_LITE_ENSURE_OK(context, ResizeOutputTensor(context, &op_context));
  }

  // TODO(nupurgarg): Change kernel implementation to take in int* instead of
  // vector<int> to remove malloc from Eval().
  // Create before and after padding arrays that are accepted by the kernel.
  std::vector<int> before_padding;
  std::vector<int> after_padding;
  const int32* paddings_data = GetTensorData<int32>(op_context.paddings);

  // TODO(nupurgarg): Change kernel implementation to use padding arrays in
  // forward order (depth, width, height, batch).
  // Build paddings in order of int[] = {batch, height, width, depth} to match
  // kernel implementation of Pad in reference_ops.h and optimized_ops.h.
  for (int idx = op_context.dims - 1; idx >= 0; --idx) {
    before_padding.push_back(paddings_data[idx * 2]);
    after_padding.push_back(paddings_data[idx * 2 + 1]);
  }

#define TF_LITE_PAD(type, scalar, pad_value)                             \
  TF_LITE_ENSURE(context, before_padding.size() <= 4);                   \
  TF_LITE_ENSURE(context, after_padding.size() <= 4);                    \
  tflite::PadParams op_params;                                           \
  op_params.left_padding_count = before_padding.size();                  \
  op_params.right_padding_count = after_padding.size();                  \
  for (int i = 0; i < op_context.dims; ++i) {                            \
    op_params.left_padding[i] = before_padding[op_context.dims - 1 - i]; \
    op_params.right_padding[i] = after_padding[op_context.dims - 1 - i]; \
  }                                                                      \
  const scalar pad_value_copy = pad_value;                               \
                                                                         \
  type::Pad(op_params, GetTensorShape(op_context.input),                 \
            GetTensorData<scalar>(op_context.input), &pad_value_copy,    \
            GetTensorShape(op_context.output),                           \
            GetTensorData<scalar>(op_context.output))
  switch (op_context.input->type) {
    case kTfLiteFloat32: {
      float pad_value = op_context.constant_values == nullptr
                            ? 0.f
                            : *GetTensorData<float>(op_context.constant_values);
      if (kernel_type == kReference) {
        TF_LITE_PAD(reference_ops, float, pad_value);
      } else {
        static_assert("Optimized ops for RISCV not implemented yet.");
      }
    } break;
    default:
      context->ReportError(context,
                           "Type %d is currently not supported by Pad.",
                           op_context.input->type);
      return kTfLiteError;
  }
#undef TF_LITE_PAD
  return kTfLiteOk;
}

}  // namespace pad

TfLiteRegistration* Register_PAD_REF() {
  static TfLiteRegistration r = {nullptr, nullptr, pad::Prepare,
                                 pad::Eval<pad::kReference>};
  return &r;
}

TfLiteRegistration* Register_PAD_OPT() {
  static TfLiteRegistration r = {nullptr, nullptr, pad::Prepare,
                                 pad::Eval<pad::kOptimized>};
  return &r;
}

TfLiteRegistration* Register_PAD() {
#ifdef RISCV
  return Register_PAD_OPT();
#else
  return Register_PAD_REF();
#endif
}

}  // namespace builtin
}  // namespace ops
}  // namespace tflite
