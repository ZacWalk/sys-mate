#include <string_view>

int main()
{
    constexpr std::string_view name = "system-stuff";
    return name.empty() ? 1 : 0;
}
