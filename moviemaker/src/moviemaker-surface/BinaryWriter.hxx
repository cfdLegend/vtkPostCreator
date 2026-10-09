#ifndef BINARY_WRITER
#define BINARY_WRITER

#include <vtkMultiBlockDataSet.h>
#include <vtkPolyData.h>

#include <limits>
#include <string>
#include <memory>

class VariableManager;
class Variable;
using VariablePtr = std::shared_ptr<Variable>;

class BinaryWriter
{
public:
    BinaryWriter(const std::string &bName, const VariableManager* vm);

    void write(const std::vector<std::tuple<std::string, vtkPolyData *>>& blocks);


private:
    std::vector<long long> _nodeOffsets;
    std::vector<std::tuple<std::string, vtkPolyData*>> _parts;
    const std::vector<VariablePtr> _vars;

    const std::string _baseName;
    const int _compactRange = std::numeric_limits<unsigned short>::max();
    int _intAccuracy = 32;
    int faceCounter;
        
    std::shared_ptr<std::ofstream> _out;

    short compactFloat(const float var, const float range)
    {
        return short(var * float(_compactRange) / range);
    }

    void writeFloat(const float var, const float min_, const float max_)
    {
        float clipped_var = var;
        clipped_var = std::min(clipped_var, max_);
        clipped_var = std::max(clipped_var, min_);
        const float range = max_ - min_;
        const float offset = (max_ + min_) * 0.5;
        short compact = compactFloat(clipped_var - offset, range);
        _out->write((char *)&compact, sizeof(short));
    }

    void writeInt(const uint var)
    {
        _out->write((char *)&var, int(_intAccuracy / 8));
    }

    void setup(const std::vector<std::tuple<std::string, vtkPolyData *>>& blocks);

    int setIntAccuracy(const std::vector<int> &faceSet);
    int setIntAccuracy(const int min_, const int max_);


};

#endif
