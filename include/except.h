#pragma once

#ifndef __cpp_exceptions
#define THROW(code) do { printf("nxcpp thread\nabort due to an error %d\n", (int)code); abort(); } while(0)
#else
#define THROW(code) throw std::system_error(code)
#endif