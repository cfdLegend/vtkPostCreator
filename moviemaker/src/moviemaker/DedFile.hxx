#ifndef DEDFILE_HEADER
#define DEDFILE_HEADER

#include <string>
#include <iostream>
#include <vector>
#include <memory>
#include <limits>
class Slicer;
class DEDMovie;
class MovieSliceData;
class Config;

class vtkBox;

class DedFileWriter
{
public:
    explicit DedFileWriter(const std::string &fName);

    void WriteHeader(const std::string &cName,
                     const uint coord, const float start, const float step,
                     const uint numFrames, const Config *conf, vtkBox *domain);
    void WriteSlice(std::shared_ptr<Slicer> slice);

private:
    short CompactFloat(const float var, const float range) const;
    void WriteFloat(const float var, const float min, const float max) const;

    std::vector<float> _minCoord;
    std::vector<float> _maxCoord;
    // as a shared ptr this will destruct and close itself
    std::shared_ptr<std::ofstream> _file; 
    const float _compactRange = std::numeric_limits<ushort>::max();

    bool _firstSlice = true;
};

#endif
