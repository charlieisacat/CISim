#include "utils.h"

std::vector<int> splitAndConvertToInt(std::string input)
{
    std::vector<int> numbers;
    std::istringstream ss(input);
    std::string token;

    while (std::getline(ss, token, ','))
    {
        try
        {
            numbers.push_back(std::stoi(token));
        }
        catch (const std::exception &e)
        {
            std::cerr << "Error converting string to int: " << e.what() << '\n';
            // Handle the error, e.g., ignore the conversion or break the loop
        }
    }

    return numbers;
}

int SatIncrement(int value, int max)
{
    if (value < max)
        value++;
    return value;
}

int SatDecrement(int value)
{
    if (value > 0)
        value--;
    return value;
}