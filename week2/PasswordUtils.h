#include <string>
#include <sstream>
#include <iomanip>

inline std::string SimpleHashPassword(const std::string& password, const std::string& salt) {
    std::string saltedPassword = password + salt;
    unsigned long hash = 0;

    for (size_t i = 0; i < saltedPassword.length(); ++i) {
        hash = hash * 31 + (unsigned char)saltedPassword[i];
    }

    for (int round = 0; round < 100; ++round) {
        hash = hash * 31 + 17;
        hash = hash ^ (hash >> 16);
    }

    std::ostringstream oss;
    oss << std::hex << std::uppercase << std::setfill('0') << std::setw(8) << hash;
    return oss.str();
}