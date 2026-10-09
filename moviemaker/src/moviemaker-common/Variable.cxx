
#include "Variable.hxx"

#include <vtkFloatArray.h>
#include <vtkPointData.h>

Variable::Variable(const std::string &fieldName, float mn, float mx) 
    : _fieldName(fieldName), _minBound(mn), _maxBound(mx)
{
}

float GetMag(const vtkFloatArray* array, const unsigned &id)
{
    auto index = id*3;
    auto x = array->GetValue(index);
    auto y = array->GetValue(index + 1);
    auto z = array->GetValue(index + 2);

    return std::sqrt(x * x + y * y + z * z);
}

float Variable::ExtractData(vtkPointData* data, unsigned index) const
{
    auto array = vtkFloatArray::SafeDownCast(data->GetAbstractArray(_fieldName.c_str()));
    if(_type == "Scalar")
    {
        return array->GetValue(index) - _offset;
    }
    else 
    {
        if(_mode == "Component")
        {
            return array->GetValue(index * 3 + _vectorComponent) - _offset;
        }
        else 
        {
            return GetMag(array, index) - _offset;
        }
    }
}

std::vector<float> Variable::ExtractDataVector(vtkPointData* data, unsigned index) const
{
    std::vector<float> ret;
    auto array = vtkFloatArray::SafeDownCast(data->GetAbstractArray(_fieldName.c_str()));
    if(_type == "Scalar")
    {
        ret.push_back(array->GetValue(index));
    }
    else 
    {
        auto id = index*3;
        ret.push_back(array->GetValue(id));
        ret.push_back(array->GetValue(id+1));
        ret.push_back(array->GetValue(id+2));
    }

    return ret;
}
