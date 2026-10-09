#include "VariableManager.hxx"
#include "Variable.hxx"

#include <fstream>
#include <iostream>
#include <filesystem>
#include <nlohmann/json.hpp>

using json = nlohmann::json;
namespace fs = std::filesystem;

VariableManager::VariableManager() = default;

void VariableManager::Load(const std::string& caseDir)
{
    Load(caseDir, "variables.json");
}

void VariableManager::Load(const std::string& caseDir, const std::string& file)
{
    fs::path c = caseDir + "/" + file;

    std::ifstream f(c);

    if(!f.is_open())
    {
        std::cout << "Failed to open " << c << std::endl;
        exit(1);
    }

    json data = json::parse(f);
    
    for(auto& [key, list] : data.items())
    {
        std::vector<VariablePtr> vars;
        for(auto& [k2, item] : list.items())
        {
            auto nv = std::make_shared<Variable>(
                item["SourceFieldName"],
                item["Min"].get<float>(),
                item["Max"].get<float>()
                );

            if(item.contains("MVFieldName"))
            {
                nv->SetMVName(item["MVFieldName"]);
            }

            if(item.contains("Type"))
            {
                nv->SetType(item["Type"]);
            }
            else 
            {
                nv->SetType("Scalar");
            }

            if(item.contains("Mode"))
            {
                nv->SetMode(item["Mode"]);
            }
            else 
            {
                nv->SetMode("Magnitude");
            }

            if(item.contains("Component"))
            {
                nv->SetVectorComponent(item["Component"].get<unsigned>());
            }
            else 
            {
                nv->SetVectorComponent(0);
            }

            if(item.contains("UseForReduction"))
            {
                nv->SetUseForReduction(item["UseForReduction"].get<bool>());
            }

            if(item.contains("ValueOffset"))
            {
                nv->SetOffset(item["ValueOffset"].get<float>());
            }

            vars.push_back(nv);
        }
        _requestedVariables.push_back(vars);
    }
}

std::vector<std::string> VariableManager::GetRequestedVariables() const
{
    std::vector<std::string> ret;
    for(const auto& c : _requestedVariables)
    {
        for(auto item : c)
        {
            ret.push_back(item->GetFieldName());
        }
    }

    return ret;
}

void VariableManager::Filter(std::vector<std::string> hasVariables)
{
    std::vector<std::vector<VariablePtr>> newList;
    for(const auto& c : _requestedVariables)
    {
        std::vector<VariablePtr> nl;
        for(auto item : c)
        {
            if(std::find(hasVariables.begin(), hasVariables.end(), item->GetFieldName()) != hasVariables.end())
            {
                nl.push_back(item);
            }
        }
        newList.push_back(nl);
    }

    _requestedVariables = newList;

    for(int i = 0; i < _requestedVariables.size(); ++i)
    {
        std::cout << "dir " << i << " extract:" << std::endl;
        std::for_each(_requestedVariables[i].begin(), _requestedVariables[i].end(), [&] (auto v) 
        {
            std::cout << "\t" << v->GetMVName() 
                    << ", from: " << v->GetFieldName()
                    << ", clip: " << v->GetMinBound() 
                    << " " << v->GetMaxBound() << std::endl;
        });
    }
}
