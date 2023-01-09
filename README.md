# San Diego C++ Meetup example for Coroutines talk

Using `conanfile.txt` with:
- liburing
- bshoshany-thread-pool

regular process - venv, install and using the following command for `gcc-12`

```shell
conan install -pr default-gcc12.2 --build=missing ..
```
and the profile looks like:
```shell
conan profile show default-gcc12.2
```
output:
```text
Configuration for profile default-gcc12.2:

[settings]
os=Linux
os_build=Linux
arch=x86_64
arch_build=x86_64
compiler=gcc
compiler.version=12.2
compiler.libcxx=libstdc++11
build_type=Release
[options]
[conf]
[build_requires]
[env]

```

# sending nc commands

example:

```shell
echo "hello world 1" | nc -nu 127.0.0.1 9090 -w0
echo "hello world 2" | nc -nu 127.0.0.1 9091 -w0
```

and the results are shown in the terminal (crc results)

# installing liburing

You can use conan.io to install liburing or your fav package manager. e.g.

```shell
sudo apt -y install liburing-dev
```

# references:
- [udp server](https://www.geeksforgeeks.org/udp-server-client-implementation-c/)
- [io_uring and coroutines](https://pabloariasal.github.io/2022/11/12/couring-1/)
