#pragma once

#include <string>

namespace classmngr::engine
{

struct NativeEnglishTeacher
{
    int id = -1;
    std::string name;
    std::string position;
    std::string phoneNumber;
    std::string birthday;
    std::string nationality;
    std::string email;
};

} // namespace classmngr::engine
