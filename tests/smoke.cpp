#include <string_view>

int main()
{
    constexpr std::string_view name = "sys-mate";
    return name.empty() ? 1 : 0;
}
