# Temp-Alloc
Single-header temporary allocator.

example:
```c++
int main()
{
    temp_init(0);

    // Game loop.
    while (true)
    {
        char* temp_memory = (char*)temp_alloc(100);

        {
            std::vector<int, temp_alloc_stl<int>> temp_vector;

            for (int i = 0; i < 100; ++i)
                temp_vector.push_back(i);
        }

        {
            typedef std::basic_string<char, std::char_traits<char>, temp_alloc_stl<char>> Temp_String;
            Temp_String temp_string  = "Andrey";

            const char* temp_c_string = temp_printf("My name is: %s!", temp_string.c_str());
            Temp_String copied_string = temp_copy_string(temp_c_string);

            copied_string.append(" I'm 119 years old!");

            std::cout << "temp_c_string: " << temp_c_string << std::endl;
            std::cout << "copied_string: " << copied_string << std::endl;
        }

        temp_reset();
    }

    temp_deinit();
}
```
