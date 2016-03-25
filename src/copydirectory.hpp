#ifndef COPYDIRECTORY_HPP
#define COPYDIRECTORY_HPP

#include <boost/filesystem.hpp>

void copyDirectory (const boost::filesystem::path& source,
                    const boost::filesystem::path& destination);

#endif
