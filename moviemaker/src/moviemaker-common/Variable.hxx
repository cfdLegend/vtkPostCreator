
#ifndef VARIABLE
#define VARIABLE

#include <string>
#include <vector>

class vtkPointData;

class Variable
{
public:
    Variable(const std::string& fieldName, float mn, float mx);

    std::string GetFieldName() const 
    {
        return _fieldName;
    }

    void SetMVName(const std::string& n)
    {
        _mvName = n;
    }

    void SetType(const std::string& t)
    {
        _type = t;
    }

    void SetMode(const std::string& m)
    {
        _mode = m;
    }

    void SetVectorComponent(unsigned c)
    {
        _vectorComponent = c;
    }

    float ExtractData(vtkPointData* data, unsigned index) const;
    std::vector<float> ExtractDataVector(vtkPointData* data, unsigned index) const;

    bool UseForReduction() const
    {
        return _useForReduction;
    }

    void SetUseForReduction(bool u)
    {
        _useForReduction = u;
    }

    float& GetMinBound()
    {
        return _minBound;
    }

    float& GetMaxBound()
    {
        return _maxBound;
    }

    std::string GetMVName() const
    {
        return _mvName;
    }

    void SetOffset(float of)
    {
        _offset = of;
    }

private:

    std::string _mvName;
    std::string _fieldName;

    std::string _type;
    std::string _mode;
    unsigned _vectorComponent;

    float _minBound;
    float _maxBound;

    bool _useForReduction;

    float _offset = 0.0;
};

#endif 
