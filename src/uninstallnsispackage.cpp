#include <util/windows/msi.hpp>

#include <boost/filesystem.hpp>
#include <boost/process.hpp>

#include <memory>
#include <string>

namespace fs = boost::filesystem;

const char* const kNsisPackageUninstallerPath = "NSISPACKAGEUNINSTALLERPATH";

extern "C" _declspec(dllexport) UINT __stdcall uninstallNsisPackage (MSIHANDLE);

extern "C" UINT __stdcall uninstallNsisPackage (MSIHANDLE handle) {
    auto session = util::windows::msi::Session{handle};
    try {
        session.log("uninstallNsisPackage");

        auto uninstaller = fs::path{session.getProperty(kNsisPackageUninstallerPath)};
        auto tmpUninstaller = fs::unique_path("nsis-uninstaller-%%%%-%%%%-%%%%-%%%%.exe");
        session.log(std::string("Copying ") + uninstaller + " -> " + tmpUninstaller);
        fs::copy_file(uninstaller, tmpUninstaller);
        std::shared_ptr<void> guard { nullptr, [&] (void*) {
            boost::system::error_code ec;
            (void)fs::remove(tmpUninstaller, ec);
            session.log(std::string("Deleted ") + tmpUninstaller + ": " + ec.message());
        }};

        using namespace boost::process;
        using namespace boost::process::initializers;

        auto packagePath = uninstaller.parent_path();
        auto child = execute(
            run_exe(tmpUninstaller),
            set_args({std::string("_?=") + packagePath})
        );

        if (wait_for_exit(child)) {
            return ERROR_INSTALL_FAILURE;
        }
        return ERROR_SUCCESS;
    }
    catch (std::exception& e) {
        session.log(std::string("Exception: ") + e.what());
        return ERROR_INSTALL_FAILURE;
    }
}