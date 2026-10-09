
#ifndef VARIABLEMANAGER
#define VARIABLEMANAGER

#include <string>
#include <vector>
#include <memory>

class Variable;
using VariablePtr = std::shared_ptr<Variable>;

class VariableManager
{
public:
    VariableManager();

    void Load(const std::string& caseDir, const std::string& file);
    void Load(const std::string& caseDir);

    std::vector<std::string> GetRequestedVariables() const;
    void Filter(std::vector<std::string> hasVariables);

    const std::vector<VariablePtr>& GetVariables(unsigned c) const
    {
        return _requestedVariables[c];
    }

private:

    std::vector<std::vector<VariablePtr>> _requestedVariables;
};

#endif 
