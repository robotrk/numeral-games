# gen_icon_header.cmake — вшивает numeral_games.png в заголовок icon_data.h (base64)
# Использование (из CMakeLists.txt):
#   add_custom_command(OUTPUT ${GEN}
#       COMMAND ${CMAKE_COMMAND} -DPNG=${SRC} -DOUT=${GEN} -P cmake/gen_icon_header.cmake ...)
if(NOT PNG OR NOT OUT)
    message(FATAL_ERROR "gen_icon_header.cmake: нужны -DPNG=<файл> -DOUT=<заголовок>")
endif()

file(READ "${PNG}" png_b64 BASE64)

# режем base64 на строки по 960 символов (безопасно для лимита строк компилятора)
string(LENGTH "${png_b64}" len)
set(chunk_size 960)
set(i 0)
set(chunks "")
while(i LESS len)
    string(SUBSTRING "${png_b64}" ${i} ${chunk_size} part)
    string(APPEND chunks "\"${part}\"\n    ")
    math(EXPR i "${i} + ${chunk_size}")
endwhile()

get_filename_component(_name "${PNG}" NAME)
file(WRITE "${OUT}"
"// Автогенерация (cmake/gen_icon_header.cmake) из ${_name}. Не редактировать вручную.\n#pragma once\nstatic const char ICON_PNG_BASE64[] =\n    ${chunks};\n")
