// ----------------------------------------------------------------------------
// -                        Open3D: www.open3d.org                            -
// ----------------------------------------------------------------------------
// The MIT License (MIT)
//
// Copyright (c) 2020 www.open3d.org
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.
// ----------------------------------------------------------------------------
//

#include <vector>

#include "open3d/ml/pytorch/TorchHelper.h"
#include "torch/custom_class.h"
#include "torch/script.h"

/// A RaggedTensor is a tensor with ragged dimensions, whose slice
/// may have different lengths. We define a container for ragged tensor
/// to support operations involving batches whose elements may have different
/// shape.
struct RaggedTensor : torch::CustomClassHolder {
public:
    RaggedTensor() {}

    /// Constructor for creating RaggedTensor with values and row_splits.
    RaggedTensor(torch::Tensor values, torch::Tensor row_splits)
        : _values(values), _row_splits(row_splits) {}

    /// Creates a RaggedTensor with rows partitioned by row_splits.
    ///
    /// The returned `RaggedTensor` corresponds with the python list defined by:
    /// ```python
    /// result = [values[row_splits[i]:row_splits[i + 1]]
    ///           for i in range(len(row_splits) - 1)]
    /// ```
    c10::intrusive_ptr<RaggedTensor> FromRowSplits(
            torch::Tensor values, torch::Tensor row_splits) const;

    /// Returns _values tensor.
    torch::Tensor GetValues() const;

    /// Returns _row_splits tensor.
    torch::Tensor GetRowSplits() const;

    /// Returns string representation.
    std::string ToString() const;

    /// Pythonic __getitem__ for RaggedTensor.
    ///
    /// Returns a slice of values based on row_splits. It can be
    /// used to retrieve i'th batch element. Currently it
    /// only supports a single integer index.
    torch::Tensor GetItem(int key) const;

    /// Pythonic __len__ for RaggedTensor.
    ///
    /// Returns number of batch elements.
    int64_t Len() const;

    /// Copy Tensor to the same device.
    c10::intrusive_ptr<RaggedTensor> Clone() const;

    c10::intrusive_ptr<RaggedTensor> Concat(
            c10::intrusive_ptr<RaggedTensor> r_tensor, int64_t axis) const;

    template <typename T>
    c10::intrusive_ptr<RaggedTensor> Add(T value) const {
        return FromRowSplits(_values + value, _row_splits);
    }

    template <typename T>
    c10::intrusive_ptr<RaggedTensor> Add_(T value) {
        _values += value;
        return c10::make_intrusive<RaggedTensor>(_values, _row_splits);
    }

    template <typename T>
    c10::intrusive_ptr<RaggedTensor> Sub(T value) const {
        return FromRowSplits(_values - value, _row_splits);
    }

    template <typename T>
    c10::intrusive_ptr<RaggedTensor> Sub_(T value) {
        _values -= value;
        return c10::make_intrusive<RaggedTensor>(_values, _row_splits);
    }

    template <typename T>
    c10::intrusive_ptr<RaggedTensor> Mul(T value) const {
        return FromRowSplits(_values * value, _row_splits);
    }

    template <typename T>
    c10::intrusive_ptr<RaggedTensor> Mul_(T value) {
        _values *= value;
        return c10::make_intrusive<RaggedTensor>(_values, _row_splits);
    }

    template <typename T>
    c10::intrusive_ptr<RaggedTensor> Div(T value) const {
        return FromRowSplits(_values / value, _row_splits);
    }

    template <typename T>
    c10::intrusive_ptr<RaggedTensor> Div_(T value) {
        _values /= value;
        return c10::make_intrusive<RaggedTensor>(_values, _row_splits);
    }

private:
    torch::Tensor _values, _row_splits;
};

static auto registry =
        torch::class_<RaggedTensor>("my_classes", "RaggedTensor")
                .def(torch::init<>())
                .def("from_row_splits", &RaggedTensor::FromRowSplits)
                .def("get_values", &RaggedTensor::GetValues)
                .def("get_row_splits", &RaggedTensor::GetRowSplits)
                .def("__repr__",
                     [](const c10::intrusive_ptr<RaggedTensor>& self) {
                         return self->ToString();
                     })
                .def("__str__",
                     [](const c10::intrusive_ptr<RaggedTensor>& self) {
                         return self->ToString();
                     })
                .def("__getitem__",
                     [](const c10::intrusive_ptr<RaggedTensor>& self,
                        int64_t key) { return self->GetItem(key); })
                .def("__len__", &RaggedTensor::Len)
                .def("clone", &RaggedTensor::Clone)
                .def("concat", &RaggedTensor::Concat)

                .def("add",
                     [](const c10::intrusive_ptr<RaggedTensor>& self,
                        torch::Tensor value) { return self->Add(value); })
                .def("add_",
                     [](const c10::intrusive_ptr<RaggedTensor>& self,
                        torch::Tensor value) { return self->Add_(value); })
                .def("__add__",
                     [](const c10::intrusive_ptr<RaggedTensor>& self,
                        torch::Tensor value) { return self->Add(value); })
                .def("__iadd__",
                     [](const c10::intrusive_ptr<RaggedTensor>& self,
                        torch::Tensor value) { return self->Add_(value); })

                .def("sub",
                     [](const c10::intrusive_ptr<RaggedTensor>& self,
                        torch::Tensor value) { return self->Sub(value); })
                .def("sub_",
                     [](const c10::intrusive_ptr<RaggedTensor>& self,
                        torch::Tensor value) { return self->Sub_(value); })
                .def("__sub__",
                     [](const c10::intrusive_ptr<RaggedTensor>& self,
                        torch::Tensor value) { return self->Sub(value); })
                .def("__isub__",
                     [](const c10::intrusive_ptr<RaggedTensor>& self,
                        torch::Tensor value) { return self->Sub_(value); })

                .def("mul",
                     [](const c10::intrusive_ptr<RaggedTensor>& self,
                        torch::Tensor value) { return self->Mul(value); })
                .def("mul_",
                     [](const c10::intrusive_ptr<RaggedTensor>& self,
                        torch::Tensor value) { return self->Mul_(value); })
                .def("__mul__",
                     [](const c10::intrusive_ptr<RaggedTensor>& self,
                        torch::Tensor value) { return self->Mul(value); })
                .def("__imul__",
                     [](const c10::intrusive_ptr<RaggedTensor>& self,
                        torch::Tensor value) { return self->Mul_(value); })

                .def("div",
                     [](const c10::intrusive_ptr<RaggedTensor>& self,
                        torch::Tensor value) { return self->Div(value); })
                .def("div_",
                     [](const c10::intrusive_ptr<RaggedTensor>& self,
                        torch::Tensor value) { return self->Div_(value); })
                .def("__div__",
                     [](const c10::intrusive_ptr<RaggedTensor>& self,
                        torch::Tensor value) { return self->Div(value); })
                .def("__idiv__",
                     [](const c10::intrusive_ptr<RaggedTensor>& self,
                        torch::Tensor value) { return self->Div_(value); })
                .def("__truediv__",
                     [](const c10::intrusive_ptr<RaggedTensor>& self,
                        torch::Tensor value) { return self->Div(value); })
                .def("__itruediv__",
                     [](const c10::intrusive_ptr<RaggedTensor>& self,
                        torch::Tensor value) { return self->Div_(value); })
                .def("__floordiv__",
                     [](const c10::intrusive_ptr<RaggedTensor>& self,
                        torch::Tensor value) { return self->Div(value); })
                .def("__ifloordiv__",
                     [](const c10::intrusive_ptr<RaggedTensor>& self,
                        torch::Tensor value) { return self->Div_(value); });
