#include "tetrio/TetrioUtils.hpp";
#include <sstream>
#include <iomanip>

uint32_t get_rank_colour(const std::string& rank) {
    if (rank == "X+") return 0xff4fd8; // pink/magenta
    if (rank == "X")  return 0x9c4dff; // purple
    if (rank == "U")  return 0xff3d00; // red (VERY important fix)

    if (rank == "SS") return 0xffd600; // gold
    if (rank == "S+") return 0xffc400;
    if (rank == "S")  return 0xfdd835;
    if (rank == "S-") return 0xc6ff00;

    if (rank == "A+") return 0x69f0ae; // green
    if (rank == "A")  return 0x00e676;
    if (rank == "A-") return 0x00c853;

    if (rank == "B+") return 0x40c4ff; // blue-cyan
    if (rank == "B")  return 0x00b0ff;
    if (rank == "B-") return 0x0091ea;

    if (rank == "C+") return 0x7c4dff; // violet
    if (rank == "C")  return 0x651fff;
    if (rank == "C-") return 0x6200ea;

    if (rank == "D+") return 0xb0bec5; // gray
    if (rank == "D")  return 0x90a4ae;
    if (rank == "D-") return 0x78909c;

    return 0x9e9e9e; // fallback
}

std::string format_double(double value, int precision) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(precision) << value;
    return oss.str();
}