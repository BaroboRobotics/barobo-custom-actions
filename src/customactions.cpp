#include "copydirectory.hpp"

#include <util/windows/msi.hpp>

#include <boost/filesystem.hpp>
#include <boost/process.hpp>

#include <memory>
#include <string>

#include <cstdint>

namespace fs = boost::filesystem;
namespace msi = util::windows::msi;

int runGuiProgram (const std::string& program, const std::string& args) {
    // Uninstallers are typically GUI programs, which means we need to do
    // something like:
    //   cmd.exe /c "start \"\" /b /wait \"c:\\temp\\nsis-uninstaller.exe\" _?=C:\\Program Files\\BaroboLink"
    // Life is hard sometimes.
    auto cmdArgs = std::vector<std::string>{
        "/c", // change to /k to leave the cmd.exe open, useful for debugging
        std::string("start \"\" /b /wait \"") + program + "\" " + args
    };
    auto cmdExe = std::string("C:\\Windows\\System32\\cmd.exe");

    using namespace boost::process;
    using namespace boost::process::initializers;
    auto child = execute(
        run_exe(cmdExe),
        set_args(cmdArgs),
        // show_window(SW_HIDE) seems to refer to the calling process' window, not the child's
        on_CreateProcess_setup([](executor& e) {
            e.creation_flags |= CREATE_NO_WINDOW;
        }),
        throw_on_error()
    );

    return wait_for_exit(child);
}

UINT uninstallNsisPackage (msi::Session& session, fs::path uninstaller) {
    auto tmpUninstaller = fs::temp_directory_path();
    tmpUninstaller /= fs::unique_path("nsis-uninstaller-%%%%-%%%%-%%%%-%%%%.exe");
    auto instDir = uninstaller.parent_path();
    auto productName = instDir.filename().string();

    auto button = session.messageBoxOkCancel(productName + " is currently installed at "
        + instDir.string() + ", and must be uninstalled before proceeding.\n\nClick OK to "
        + "uninstall " + productName + ".");
    if (msi::Session::Button::CANCEL == button) {
        session.log("User pressed cancel, aborting.");
        return ERROR_INSTALL_USEREXIT;
    }

    session.log(std::string("Copying ") + uninstaller.string() + " -> " + tmpUninstaller.string());
    fs::copy_file(uninstaller, tmpUninstaller);
    std::shared_ptr<void> guard { nullptr, [&] (void*) {
        // Delete the file on scope exit, regardless of what happens
        boost::system::error_code ec;
        (void)fs::remove(tmpUninstaller, ec);
        session.log(std::string("Deleted ") + tmpUninstaller.string() + ": " + ec.message());
    }};

    if (runGuiProgram(tmpUninstaller.string(), std::string("_?=") + instDir.string())) {
        return ERROR_INSTALL_FAILURE;
    }
    return ERROR_SUCCESS;
}

// uninstallBaroboLink needs elevated privileges, thus it needs to be
// deferred, thus it runs after all the public properties like
// BAROBOLINKUNINSTALLER have disappeared, thus we need to use the trick
// described here (https://www.firegiant.com/wix/tutorial/events-and-actions/at-a-later-stage/)
// to get our argument.
const char* const kBaroboLinkUninstaller = "CustomActionData";

// __declspec(dllexport) will expose uninstallNsisPackage adorned as
// _uninstallNsisPackage@4 (note: sizeof(MSIHANDLE) == 4, thus the @4). To
// expose the unadorned symbol, we need this pragma. Yup yup.
#pragma comment(linker, "/EXPORT:uninstallBaroboLink=_uninstallBaroboLink@4")
extern "C" __declspec(dllexport) UINT __stdcall uninstallBaroboLink (MSIHANDLE handle) {
    auto session = msi::Session{handle};
    try {
        session.log("uninstallBaroboLink");

        auto uninstaller = fs::path{session.getProperty(kBaroboLinkUninstaller)};
        return uninstallNsisPackage(session, uninstaller);
    }
    catch (std::exception& e) {
        session.messageBoxOk(std::string("Exception: ") + e.what());
        return ERROR_INSTALL_FAILURE;
    }
}

const char* const kBaromeshdPath = "CustomActionData";

#pragma comment(linker, "/EXPORT:uninstallOldLinkbotLabs=_uninstallOldLinkbotLabs@4")
extern "C" __declspec(dllexport) UINT __stdcall uninstallOldLinkbotLabs (MSIHANDLE handle) {
    auto session = msi::Session{handle};
    try {
        session.log("uninstallOldLinkbotLabs");

        auto baromeshd = fs::path{session.getProperty(kBaromeshdPath)};
        return uninstallNsisPackage(session, baromeshd.parent_path() / "Uninstall.exe");
    }
    catch (std::exception& e) {
        session.messageBoxOk(std::string("Exception: ") + e.what());
        return ERROR_INSTALL_FAILURE;
    }
}

const char* const kSourceDestinationPathPair = "CustomActionData";

#pragma comment(linker, "/EXPORT:copyChBinding=_copyChBinding@4")
extern "C" __declspec(dllexport) UINT __stdcall copyChBinding (MSIHANDLE handle) {
    auto session = msi::Session{handle};
    try {
        session.log("copyChBinding");

        // Our source;destination pair will be something like this:
        //   C:/Program Files (x86)/Linkbot Labs/chbarobo/win64/chbarobo/;C:/Ch
        auto srcDestPair = session.getProperty(kSourceDestinationPathPair);
        auto semicolon = srcDestPair.find(';');
        // chbarobo is provided with a trailing slash, so we actually want its parent
        auto chbaroboPath = fs::path(srcDestPair.substr(0, semicolon)).parent_path();
        auto chPath = fs::path(srcDestPair.substr(semicolon + 1));

        session.log(std::string("Installing ") + chbaroboPath.string() + " to " + chPath.string());

        fs::remove_all(chPath / "package" / chbaroboPath.filename());
        copyDirectory(chbaroboPath, chPath / "package" / chbaroboPath.filename());
        fs::copy_file(chbaroboPath / "include" / "linkbot.h",
                chPath / "toolkit" / "include" / "linkbot.h",
                fs::copy_option::overwrite_if_exists);

        return ERROR_SUCCESS;
    }
    catch (std::exception& e) {
        session.messageBoxOk(std::string("Exception: ") + e.what());
        return ERROR_INSTALL_FAILURE;
    }
}

// the DPInst command line comes packaged like:
//   C:\Path\To\Program\Executable.exe;arg0 arg1 arg2
// Everything before the semicolon should be interpreted as the program to run, everything after,
// its arguments.
const char* const kDpinstCommandLine = "CustomActionData";
static const uint32_t kDpinstErrorFlag = 0x80000000;
static const uint32_t kDpinstRebootFlag = 0x40000000;

#pragma comment(linker, "/EXPORT:installLinkbotDriver=_installLinkbotDriver@4")
extern "C" __declspec(dllexport) UINT __stdcall installLinkbotDriver (MSIHANDLE handle) {
    auto session = msi::Session{handle};
    try {
        session.log("installLinkbotDriver");
        auto cmdline = session.getProperty(kDpinstCommandLine);
        auto semicolon = cmdline.find(';');
        // The program is everything up to the semicolon
        auto program = cmdline.substr(0, semicolon);
        // The arguments are everything after it, if anything
        auto args = semicolon != std::string::npos
                    ? cmdline.substr(semicolon + 1)
                    : "";
        session.log(std::string("Running ") + program + " " + args);
        auto rc = runGuiProgram(program, args);
        session.log(std::string("DPInst return code: ") + std::to_string(rc));
        if (rc & kDpinstErrorFlag) {
            session.messageBoxOk(std::string("DPInst error flag set: ") + std::to_string(rc));
            return ERROR_INSTALL_FAILURE;
        }
        if (rc & kDpinstRebootFlag) {
            session.messageBoxOk(std::string("DPInst reboot flag set: ") + std::to_string(rc));
            return ERROR_INSTALL_FAILURE;
        }
        return ERROR_SUCCESS;
    }
    catch (std::exception& e) {
        session.messageBoxOk(std::string("Exception: ") + e.what());
        return ERROR_INSTALL_FAILURE;
    }
}
