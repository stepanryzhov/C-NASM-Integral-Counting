CC = gcc # наш великий и всемогущий компилятор	
NASM = nasm 

SPEC_FILE ?= in.txt # строчка говорит: если SPEC_FILE не задан снаружи, использовать in.txt

# Флаги компиляции
# -std=c11 использовать стандарт C11.
# -O2 включает оптимизации
CFLAGS = -std=c11 -O2

# C32FLAGS — флаги для 32-битной компиляции main.c:
# $(CFLAGS) подставляет -std=c11 -O2 
# и добавляются -m32 -fno-pie

# -m32 говорит что собираем 32битный объектник
# -fno-pie позволяет линковать с 32 битным NASM  (по умному: не делать позиционно-независимый объект)
C32FLAGS = $(CFLAGS) -m32 -fno-pie

#LDFLAGS — флаги линковки:
LDFLAGS = -m32 -no-pie

#NASMFLAGS — флаги для NASM:
NASMFLAGS = -f elf32

# Это говорит make, что all, clean, test — это не настоящие файлы, а команды-цели.
# Без .PHONY если вдруг в папке появится файл с именем clean, команда может работать неправильно

.PHONY: all clean test

# главная цель - цель по умолчанию
# то есть если написать просто make то утилита выполнит именно эту секцию - integral
all: integral

#Эта секция собирает программу generator из файла generator.c.
# $@ означает имя цели. Здесь цель — generator
# $< означает первую зависимость. Здесь первая зависимость — generator.c
# -lm подключает математическую библиотеку.
generator: generator.c
	$(CC) $(CFLAGS) -o $@ $< -lm

# Чтобы получить:
# funcs.asm
# generated_spec.h
# нужно, чтобы уже были:
# generator
# in.txt

#Эта секция собирает .asm и .h файлы с помощью generator
funcs.asm generated_spec.h: generator $(SPEC_FILE)
	./generator $(SPEC_FILE) funcs.asm generated_spec.h 

# Эта секция собирает объектник main c помощью generated_spec.h
# -c нужен как раз для того чтобы не собирать как готовую программу, оставить только объектник
main.o: main.c generated_spec.h
	$(CC) $(C32FLAGS) -c -o $@ main.c 

# собирает объектник ассемблера 
funcs.o: funcs.asm
	$(NASM) $(NASMFLAGS) -o $@ funcs.asm

# ключевая секция которая собирает исполняемый файл используя объектники main.o и funcs.o
integral: main.o funcs.o
	$(CC) $(LDFLAGS) -o $@ main.o funcs.o -lm

#запуск тестов используя исполняемый файл integral
test: integral
	./integral --test-root 1:2:0.0:4.0:0.000001:2.0
	./integral --test-root 3:2:1.0:2.0:0.000001:1.4142135624
	./integral --test-root 1:4:0.0:3.0:0.000001:1.5
	./integral --test-integral 1:0.0:2.0:0.000001:2.0
	./integral --test-integral 2:1.0:3.0:0.000001:4.0
	./integral --test-integral 3:0.0:3.0:0.000001:9.0

#чистит плоды предыдущей сборки
clean:
	rm -f *.o integral generator funcs.asm generated_spec.h
