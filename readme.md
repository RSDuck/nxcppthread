# nxcppthread

A partial implementation of the C++11 threading library for Nintendo Switch, directly using [libnx](https://github.com/switchbrew/libnx).

## Current status

- `<thread>`
    - done
- `<mutex>`
    - `mutex`, `recursive_mutex`, `lock_guard`, `lock` and `unlock` are done
- `<condition_variable>`
    - everything except `condition_variable_any`

The library is currently only lightly tested, you're probably going to encounter small errors in every corner.

## Credits

* libnx contributors
* cppreference.com and cplusplus.com