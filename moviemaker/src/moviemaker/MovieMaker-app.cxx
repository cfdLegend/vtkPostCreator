
#include "MovieMaker.hxx"
#include "TicTok.hxx"
#include "VariableManager.hxx"
#include "VtkLoader.hxx"
#include "Config.hxx"

#include <vtkObject.h>
#include <vtkLogger.h>

#include <sstream>
#include <filesystem>
namespace fs = std::filesystem;

void writeUsage()
{
    std::cout << "USAGE: WF1MovieMaker <casename>" << std::endl;
    std::cout << "Options: -fullcar (-fc) - you are running a full car (rather than a half car)" << std::endl;
    std::cout << "         -frontbrakeduct (-fbd)    - if you are running movies for the front brake duct" << std::endl;
    std::cout << "         -rearbrakeduct (-rbd)    - if you are running movies for the rear brake duct" << std::endl;
    std::cout << "         -title    - the movie title" << std::endl;
    std::cout << "         -casedir    - case run directory" << std::endl;
    std::cout << "         -outputdir    - the full path to the output movie directory" << std::endl;
    std::cout << "         -fileprefix   - (optional) set a prefex to the default file name" << std::endl;
    std::cout << "         -email   - sends an email to this address when done" << std::endl;
    std::cout << "         -h    - this message" << std::endl;
    exit(1);
}

int main(int argv, char **argc)
{
    // read in ensight data...
    if (argv < 2)
        writeUsage();

    vtkLogger::SetStderrVerbosity(vtkLogger::Verbosity::VERBOSITY_ERROR);
    vtkObject::GlobalWarningDisplayOff();

    TicTok time_total;
    time_total.Start();

    bool fullCar = false;
    bool appendTitle = false;
    bool frontBrake = false;
    bool rearBrake = false;

    std::string outputDir = "MOVIES";
    std::string title;
    std::string configType = "std";
    std::string filePrefix = "";

    for (int i = 1; i < argv; i++)
    {
        if (strcmp(argc[i], "-h") == 0 || strcmp(argc[i], "-help") == 0)
        {
            writeUsage();
        }
        else if (strcmp(argc[i], "-fc") == 0 || strcmp(argc[i], "-fullcar") == 0)
        {
            appendTitle = false;
            fullCar = true;
        }
        else if (strcmp(argc[i], "-fbd") == 0 || strcmp(argc[i], "-frontbrakeduct") == 0)
        {
            appendTitle = false;
            frontBrake = true;
        }
        else if (strcmp(argc[i], "-rbd") == 0 || strcmp(argc[i], "-rearbrakeduct") == 0)
        {
            appendTitle = false;
            rearBrake = true;
        }
        else if (strcmp(argc[i], "-outputdir") == 0)
        {
            appendTitle = false;
            if (i == argv - 1)
            {
                std::cerr << "You need to provide an output directory if you specify the -outputdir option" << std::endl;
                exit(0);
            }

            i++;
            outputDir = std::string(argc[i]);
            std::cout << "Setting output directory to: " << outputDir << std::endl;
        }
        else if (strcmp(argc[i], "-fileprefix") == 0)
        {
            if (i == argv - 1)
            {
                std::cerr << "You need to provide a file prefix if you specify the -filename option" << std::endl;
                exit(0);
            }

            i++;
            filePrefix = std::string(std::string(argc[i]) + "_");
            std::cout << "Setting file prefix to: " << filePrefix << std::endl;
        }
        else if (strcmp(argc[i], "-title") == 0)
        {
            appendTitle = true;
            if (i == argv - 1)
            {
                std::cerr << "You need to provide a title if you specify the -title option" << std::endl;
                exit(0);
            }

            i++;
            title = std::string(argc[i]);
        }
        else if (appendTitle)
        {
            title += " " + std::string(argc[i]);
        }
    }

    int counterOption = 0;
    counterOption += fullCar ? 1 : 0;
    counterOption += frontBrake ? 1 : 0;
    counterOption += rearBrake ? 1 : 0;

    if (counterOption > 1)
    {
        std::cout << "Options miss match... You appear to have specified more than 1 model option (-fullcar, -frontbrakeduct, -rearbrakeduct)";
        exit(1);
    }

    std::string fcsuffix = fullCar ? "_fc" : "";
    if (frontBrake)
    {
        configType = "fbd";
        fullCar = true;
    }
    else if (rearBrake)
    {
        configType = "rbd";
        fullCar = true;
    }

    // check the output directory exists
    if (!fs::exists((fs::path)outputDir))
    {
        std::cout << "Output directory: " << outputDir << " does not exist - exiting" << std::endl;
        exit(1);
    }

    //////////////////////////
    //////////////////////////
    // done command line parseing now do work
    //////////////////////////
    //////////////////////////

    auto vm = std::make_shared<VariableManager>();
    vm->Load(std::string(argc[1]));

    auto vtkLoader = std::make_shared<VtkLoader>(vm.get());
    vtkLoader->Setup(std::string(argc[1]));    

    Config config;
    config.ReadGlobalSettings(std::string(argc[1]), configType, fcsuffix);
    config.ReadDediSettings(std::string(argc[1]));

    MovieMaker movie(&config, fullCar, frontBrake || rearBrake, outputDir, title, filePrefix);

    movie.MakeMovies(vtkLoader.get(), vm.get());

    time_total.Stop();

    std::cout << std::endl;
    time_total.Report("ALL");

    return 0;
}
