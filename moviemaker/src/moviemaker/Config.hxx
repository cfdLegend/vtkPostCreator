
#ifndef CONFIG_HXX
#define CONFIG_HXX

#include <string>
#include <vector>

struct SliceOptions
{
    float Start;
    float End;
    float Split;
    std::string Prefix = "";
};

class Config
{
public:
    void ReadGlobalSettings(const std::string& caseDir, 
                            const std::string& configType, 
                            const std::string& fcsuffix);
    void ReadDediSettings(const std::string& caseDir);

    SliceOptions* GetSliceOption(unsigned direction)
    {
        return &_sliceOptions[direction];
    }

    const float& GetPitch() const
    {
        return _pitch;
    }

    const float& GetRoll() const
    {
        return _roll;
    }

    const float& GetYaw() const
    {
        return _yaw;
    }

    const float& GetRefVel() const
    {
        return _refVel;
    }

    const float& GetVelXIn() const
    {
        return _velXin;
    }

    const float& GetVelYIn() const
    {
        return _velYin;
    }

    const float& GetVelZIn() const
    {
        return _velZin;
    }

private:

    std::vector<SliceOptions> _sliceOptions;

    float _pitch = 0.0;
    float _roll = 0.0;
    float _yaw = 0.0;
    float _refVel = 55.0;

    float _velXin = 0.0;
    float _velYin = 0.0;
    float _velZin = 0.0;
};

#endif
