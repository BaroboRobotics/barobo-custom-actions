#include <util/windows/msi.hpp>

#include <boost/filesystem.hpp>
#include <boost/process.hpp>

#include <memory>
#include <string>

namespace fs = boost::filesystem;
namespace msi = util::windows::msi;

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

    using namespace boost::process;
    using namespace boost::process::initializers;

    // The uninstaller is a GUI program, which means we need to do
    // something like:
    //   cmd.exe /c "start \"\" /b /wait \"c:\\temp\\nsis-uninstaller.exe\" _?=C:\\Program Files\\BaroboLink"
    // Life is hard sometimes.
    auto args = std::vector<std::string>{
        "/c", // change to /k to leave the cmd.exe open, useful for debugging
        std::string("start \"\" /b /wait \"") + tmpUninstaller.string() + "\" "
            + std::string("_?=") + instDir.string()
    };
    auto cmdExe = std::string("C:\\Windows\\System32\\cmd.exe");
    session.log(std::string("Executing ") + cmdExe + " " + args[0] + " " + args[1]);
    auto child = execute(
        run_exe(cmdExe),
        set_args(args),
        // show_window(SW_HIDE) seems to refer to the calling process' window
        on_CreateProcess_setup([](executor& e) {
            e.creation_flags |= CREATE_NO_WINDOW;
        }),
        throw_on_error()
    );

    if (wait_for_exit(child)) {
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

const char* const kLinkbotDotHPath = "CustomActionData";

#pragma comment(linker, "/EXPORT:copyLinkbotDotH=_copyLinkbotDotH@4")
extern "C" __declspec(dllexport) UINT __stdcall copyLinkbotDotH (MSIHANDLE handle) {
    auto session = msi::Session{handle};
    try {
        session.log("copyLinkbotDotH");

        auto from = fs::path{session.getProperty(kLinkbotDotHPath)};
        auto to = from.parent_path() // Ch/package/chbarobo/include
                      .parent_path() // Ch/package/chbarobo
                      .parent_path() // Ch/package
                      .parent_path() // Ch
                      / "toolkit"    // Ch/toolkit
                      / "include"    // Ch/toolkit/include
                      / from.filename();
        fs::copy_file(from, to, fs::copy_option::overwrite_if_exists);
        return ERROR_SUCCESS;
    }
    catch (std::exception& e) {
        session.messageBoxOk(std::string("Exception: ") + e.what());
        return ERROR_INSTALL_FAILURE;
    }
}
