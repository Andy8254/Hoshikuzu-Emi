#include "tetrio/TetrioUtils.hpp";
#include <sstream>
#include <iomanip>

uint32_t get_rank_colour(const std::string& rank) {
    if (rank == "X+") return 0xFF1A1A;
    if (rank == "X")  return 0xFF4D4D;
    if (rank == "U")  return 0xFF9900;
    if (rank == "SS") return 0xFFD700;
    if (rank == "S+" || rank == "S" || rank == "S-") return 0x00FFFF;
    if (rank == "A+" || rank == "A" || rank == "A-") return 0x00FF00;
    if (rank == "B+" || rank == "B" || rank == "B-") return 0x9966FF;
    if (rank == "C+" || rank == "C" || rank == "C-") return 0xAAAAAA;
    if (rank == "D+" || rank == "D" || rank == "D-") return 0x777777;
    if (rank == "Z") return 0x444444; // Unranked
    return 0x5865F2; // fallback
}

std::string format_double(double value, int precision) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(precision) << value;
    return oss.str();
}