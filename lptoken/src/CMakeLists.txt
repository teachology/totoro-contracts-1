project(lptoken)

set(EOSIO_WASM_OLD_BEHAVIOR "Off")
find_package(eosio.cdt)

add_contract( lptoken lptoken lptoken.cpp )
target_include_directories( lptoken PUBLIC ${CMAKE_SOURCE_DIR}/../include )
target_ricardian_directory( lptoken ${CMAKE_SOURCE_DIR}/../ricardian )