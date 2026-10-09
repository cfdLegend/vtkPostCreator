
#ifndef VTKLOADER
#define VTKLOADER

#include <string>

class VariableManager;

class vtkAlgorithm;

class VtkLoader
{
public:
    explicit VtkLoader(VariableManager* vm) : _variables(vm), _filter(true)
    {

    }

    VtkLoader(VariableManager* vm, bool filter) : _variables(vm), _filter(filter)
    {

    }
    
    void Setup(const std::string& directory);
    void Setup(const std::string& directory, const std::string& ignore);

    vtkAlgorithm* Update();

private:

    bool IsOpenFoam(const std::string& directory) const;
    void SetupOpenFoam(const std::string& directory, const std::string& ignore);
    void SetupEnsight(const std::string& directory);

    vtkAlgorithm* _reader;
    VariableManager* _variables;

    bool _filter;
    double _time = -1.0;
};

#endif
