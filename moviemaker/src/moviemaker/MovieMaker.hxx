#ifndef MOVIEMAKER_HXX
#define MOVIEMAKER_HXX

#include "TicTok.hxx"

#include <vector>
#include <map>
#include <memory>

class triangulateio;
class Variable;
using VariablePtr = std::shared_ptr<Variable>;
class Config;
class SliceOptions;
class VariableManager;
class VtkLoader;
using TicTokPtr = std::shared_ptr<TicTok>;

class vtkMultiBlockDataSet;
class vtkBox;

class MovieMaker
{
public:

    MovieMaker(Config* conf,
               const bool fc, const bool bd, const std::string &out, 
               const std::string &cName,
               const std::string &filePrefix);

    void MakeMovies(VtkLoader* reader, VariableManager* vm);

private:

    void CreateMovie(const int coord, 
                     const SliceOptions* sliceOptions, 
                     const std::vector<VariablePtr>& vars,
                     const std::string& fileName,
                     vtkMultiBlockDataSet* _domain);

    const std::string _caseName;

    Config* _conf;
    const std::string _filePrefix; 
    vtkBox* _boxOfInterest;

    const std::string _outputDir;

    bool _fullCar;
    bool _brakeDuct;

    TicTok _dedfilewrite;
    TicTok _chunking;

    std::map<std::string, TicTokPtr, std::less<>> _sliceTimers;
    
};

#endif
