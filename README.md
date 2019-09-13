# Introduction (see http://bashgen.vallua.ru for web example)
Common purpose of this system - generating new sequences based on old ones. Its features:
1. Custom backends - use one of included or write your own.
2. Multithreading - you don't need to do it by youself - only mutexes.
3. Cache - API implements simple save/load functions + append mode from cmd.

# Note
I created this project for my own needs. You can send issues, but probably they will be ignored.

# Building
    mkdir build
    cd build
    cmake ..
    make -j 4
# Frontends (in bin/)
## bin/generators
Main frontend. Run with `--help` to see help. Supports any backends, cache r/w/a, training/generating.

## bin/markovSQLClient
Special light client - config fully compatible with backend. Accepts one argument - config file.  
Output is always valid and contains ONLY generated text.  
Created for websever with limited deps.

# Backends (in lib/)
## lib/libtestBackend.so
Test backend. Only for test.

## lib/libmarkovBackend.so
Useful markov chain. Pass `-phelpme` to display help. Stores all data in memory (`map`/`unordered_map`).

## lib/libmarkovSqlBackend.so
Useful markov chain. Pass `-phelpme` to display help. Stores all data in MySQL (you need server).
## lib/libmarkovSQLite.so
Useful markov chain. Pass `-phelpme` to display help. Stores all data in SQLite3 database.

It is most advanced backend: it use extra dictionary table to store all possible text fragments, and main table only store its `rowid`'s. Different data sets can share one dictionary table (it is universal). Also, it is fastest one and by default work in memory. I strongly recommend to use this one.
