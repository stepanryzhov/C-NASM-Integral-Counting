/*
 * Точка входа вспомогательной программы generator - main
 *
 * generator запускается не пользователем вручную, а из Makefile.
 * Его задача — прочитать файл SPEC_FILE с описанием функций,
 * построить деревья выражений и производных, а затем сгенерировать:
 *
 *     1. OUT_ASM    — NASM-файл с функциями f1, f2, f3, df1, df2, df3;
 *     2. OUT_HEADER — C-заголовок с границами отрезка SPEC_A и SPEC_B.
 *
 * Формат запуска:
 *
 *     ./generator SPEC_FILE OUT_ASM OUT_HEADER
 *
 * Например:
 *
 *     ./generator in.txt funcs.asm generated_spec.h
 *
 */

#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// мало ли не определены в math.h (не во всех стандартах обязаны быть)
#define CONST_PI 3.14159265358979323846
#define CONST_E  2.71828182845904523536

//Type описывает тип узла в дереве выражения.
typedef enum {
    NODE_NUM, NODE_X, NODE_ADD, NODE_SUB, NODE_MUL, NODE_DIV, NODE_SIN, NODE_COS, NODE_TAN, NODE_CTG
} Type; 

//Node — один узел дерева выражения.
typedef struct Node {
    Type type; // тип узла: число, x, операция и т.д.

    double value; // значение числа, используется только если type == NODE_NUM

    struct Node *left;  //левый потомок:
                        // для бинарных операций это левый операнд;
                        // для унарных операций это единственный операнд;
                        //  для числа и x обычно NULL.

    struct Node *right; // правый потомок:
                        // используется только для бинарных операций;
                        // для унарных операций, числа и x обычно NULL.
} Node; 

static Node *create_node(Type k, double v, Node *l, Node *r) {
    /// Самая обычная функция для создания узла в дереве
    /// Аргументы: Type k - тип узла; double v - значение узла (если число); Node *l, *r - левые и правые потомки
    /// Функция возвращает Node * - указатель на созданный узел 

    Node *p = (Node *)calloc(1, sizeof(Node));
    if (!p) 
    { 
        fprintf(stderr, "out of memory\n"); exit(1); 
    }
    p->type = k; 
    p->value = v; 
    p->left = l; 
    p->right = r;
    return p;
}

//  Короткие функции-обертки над create_node.
//  Они нужны, чтобы не писать каждый раз create_node(...) вручную 
//  и сделать код построения дерева более читаемым.
//  все возвращают указатель на созданный узел дерева

static Node *num(double x) 
{ 
    return create_node(NODE_NUM, x, NULL, NULL); 
}
static Node *var(void) 
{ 
    return create_node(NODE_X, 0.0, NULL, NULL); 
}
static Node *un(Type k, Node *a) 
{ 
    return create_node(k, 0.0, a, NULL); 
}
static Node *bin(Type k, Node *a, Node *b) 
{ 
    return create_node(k, 0.0, a, b); 
}


    
static Node *copy_tree(const Node *n) {
    // самая обычная рекурсивная функция для копирования дерева 
    // Аргументы: const Node *n -  указатель на узел; 
    // Функция возвращает: Node * - новый узел с такими же полями + копирует все дерево под ним

    if (!n){
        return NULL;
    }
    return create_node(n->type, n->value, copy_tree(n->left), copy_tree(n->right));
}


static int is_num(const Node *n, double x) {
    // Эта функция проверяет: является ли узел числом, примерно равным x.
    // Аргументы: const Node *n - узел; double x - число
    // Функция возвращает: int 
    // 1 - да
    // 0 - нет

    return n && n->type == NODE_NUM && fabs(n->value - x) < 1e-14;
}

static Node *derivative(const Node *n) {

    // Функция строит дерево выражения производной.
    // Аргументы: const Node *n - указатель на начало дерева выражения
    // Функция возвращает: Node * - указатель на начало постоенного дерева

    /* Производная константы равна 0. */
    if (n->type == NODE_NUM) {
        return num(0.0);
    }

    /* Производная x равна 1. */
    else if (n->type == NODE_X) {
        return num(1.0);
    }

    
    //  * Производная суммы:
    //  * (u + v)' = u' + v'
    else if (n->type == NODE_ADD) {
        return bin(NODE_ADD,
                derivative(n->left),
                derivative(n->right));
    }

    // Производная разности:
    // (u - v)' = u' - v'
    else if (n->type == NODE_SUB) {
        return bin(NODE_SUB,
                derivative(n->left),
                derivative(n->right));
    }

    // Производная произведения:
    // (u * v)' = u' * v + u * v'
    else if (n->type == NODE_MUL) {
        return bin(NODE_ADD,
                bin(NODE_MUL,
                    derivative(n->left),
                    copy_tree(n->right)),
                bin(NODE_MUL,
                    copy_tree(n->left),
                    derivative(n->right)));
    }
    
    // Производная частного:
    // (u / v)' = (u' * v - u * v') / (v * v)
    else if (n->type == NODE_DIV) {
        return bin(NODE_DIV,
                bin(NODE_SUB,
                    bin(NODE_MUL,
                        derivative(n->left),
                        copy_tree(n->right)),
                    bin(NODE_MUL,
                        copy_tree(n->left),
                        derivative(n->right))),
                bin(NODE_MUL,
                    copy_tree(n->right),
                    copy_tree(n->right)));
    }

    // Производная синуса:
    // (sin u)' = cos(u) * u'
    else if (n->type == NODE_SIN) {
        return bin(NODE_MUL,
                un(NODE_COS, copy_tree(n->left)),
                derivative(n->left));
    }

    // Производная косинуса:
    // (cos u)' = -sin(u) * u'
    else if (n->type == NODE_COS) {
        return bin(NODE_MUL,
                bin(NODE_MUL,
                    num(-1.0),
                    un(NODE_SIN, copy_tree(n->left))),
                derivative(n->left));
    }

    
    // Производная тангенса:
    // (tan u)' = u' / cos^2(u)

    else if (n->type == NODE_TAN) {
        return bin(NODE_DIV,
                derivative(n->left),
                bin(NODE_MUL,
                    un(NODE_COS, copy_tree(n->left)),
                    un(NODE_COS, copy_tree(n->left))));
    }

    // Производная котангенса:
    // (ctg u)' = -u' / sin^2(u)
    else if (n->type == NODE_CTG) {
        return bin(NODE_DIV,
                bin(NODE_MUL,
                    num(-1.0),
                    derivative(n->left)),
                bin(NODE_MUL,
                    un(NODE_SIN, copy_tree(n->left)),
                    un(NODE_SIN, copy_tree(n->left))));
    }

    else {
        return num(0.0);
    }
}

static Node *simplify(Node *n)
{
    // Фунция упрощает дерево выражения.
    // Она только убирает очевидно лишние операции:
    //      x + 0 -> x
    //      0 + x -> x
    //      x - 0 -> x
    //      x * 0 -> 0
    //      0 * x -> 0
    //      x * 1 -> x
    //      1 * x -> x
    //      0 / x -> 0
    //      x / 1 -> x
    // Аргументы: Node *n - дерево выражения 
    // Функция возвращает: Node * - указатель на начало упрощенного дерева


    if (n == NULL) {
        return NULL;
    }

    // Сначала упрощаем левое и правое поддерево.
    // Поэтому упрощение идёт снизу вверх:
    // сначала дети, потом текущий узел.
    n->left = simplify(n->left);
    n->right = simplify(n->right);

    // Упрощения для сложения:
    //     0 + x -> x
    //     x + 0 -> x
    if (n->type == NODE_ADD) {
        if (is_num(n->left, 0.0)) {
            return n->right;
        }
        if (is_num(n->right, 0.0)) {
            return n->left;
        }
    }

    // Упрощение для вычитания:
    //     x - 0 -> x
    else if (n->type == NODE_SUB) {
        if (is_num(n->right, 0.0)) {
            return n->left;
        }
    }

    // Упрощения для умножения:
    //     0 * x -> 0
    //     x * 0 -> 0
    //     1 * x -> x
    //     x * 1 -> x
    else if (n->type == NODE_MUL) {
        if (is_num(n->left, 0.0) || is_num(n->right, 0.0)) {
            return num(0.0);
        }
        if (is_num(n->left, 1.0)) {
            return n->right;
        }
        if (is_num(n->right, 1.0)) {
            return n->left;
        }
    }

    // Упрощения для деления:
    //     0 / x -> 0
    //     x / 1 -> x
    // Деление на 0 здесь специально не обрабатывается.
    // Если такое выражение пришло во входном файле,
    // это ошибка области определения функции.

    else if (n->type == NODE_DIV) {
        if (is_num(n->left, 0.0)) {
            return num(0.0);
        }
        if (is_num(n->right, 1.0)) {
            return n->left;
        }
    }

    // Если ни одно правило не подошло,
    // возвращаем исходный узел без изменений.
    return n;
}

static int read_number(const char *s, double *out)
// Это вспомогательная функция, которая проверяет
// является ли строка s корректным вещественным числом
// Она нужна в parse_rpn, чтобы отличать числовые токены от остальных токенов
// Аргументы: 
//   const char *s — строка, которую проверяем.
//   double *out — адрес переменной, куда надо записать найденное число.
// Функция возвращает: int
//   1 — строка является числом
//   0 — строка не является числом

{
    char *end; //   нужен для strtod. В него strtod запишет адрес символа, на котором остановилось чтение числа.
    double x = strtod(s, &end); // strtod означает “string to double”: преобразовать строку в double.

    if (end != s && *end == '\0') { // end != s — strtod действительно что-то прочитал. 
                                    // *end == '\0' — после числа ничего не осталось. Это важно, чтобы строка "2abc" не считалась числом.
        *out = x;                   
        return 1;
    }

    return 0;
}

static Node *parse_funcstrings(char *line, int line_no) {
// Функция берёт одну строку с функцией в обратной польской записи и превращает её в дерево выражения

// Аргументы: char *line - строка с выражением
//            int line_no - номер строки в файле. Он нужен только для сообщений об ошибках.
//
// Функция возвращает: Node * - указатель на корень дерева выражения

        
    Node *stack[4096];
    int sp = 0;

    char *t = strtok(line, " \t\r\n"); // 

    while (t != NULL) {
        double x;
        if (!strcmp(t, "x")) // проверка на переменную x
        {
            stack[sp++] = var();
        }
        
        else if (!strcmp(t, "e")) // проверка на e
        {
            stack[sp++] = num(CONST_E);
        }

        else if (!strcmp(t, "pi")) // проверка на пи
        {
            stack[sp++] = num(CONST_PI);
        } 

        else if (read_number(t, &x)) // проверка на число
        {
            stack[sp++] = num(x);
        }
         
        else if (!strcmp(t, "+") || !strcmp(t, "-") || !strcmp(t, "*") || !strcmp(t, "/"))
        {
            if (sp < 2)
            { 
                fprintf(stderr, "line %d: not enough operands for %s\n", line_no, t); exit(1);
            }

            Node *b = stack[--sp]; // считали правый операнд 
            Node *a = stack[--sp]; // считали левый операнд
            Type k;
            
            // назначаем тип операции
            if (strcmp(t, "+") == 0) 
            {
                k = NODE_ADD;
            } 
            else if (strcmp(t, "-") == 0) 
            {
                k = NODE_SUB;
            } 
            else if (strcmp(t, "*") == 0) 
            {
                k = NODE_MUL;
            } 
            else 
            {
                k = NODE_DIV;
            }
            stack[sp++] = bin(k, a, b); // добавили на стек новую операцию
        } 

        else if (!strcmp(t, "sin") || !strcmp(t, "cos") || !strcmp(t, "tan") || !strcmp(t, "ctg"))
        {
            if (sp < 1) 
            { 
                fprintf(stderr, "line %d: not enough operands for %s\n", line_no, t); exit(1); 
            }
            Node *a = stack[--sp]; // считали операнд
            Type k;

            if (strcmp(t, "sin") == 0) 
            {
                k = NODE_SIN;
            } 
            else if (strcmp(t, "cos") == 0) 
            {
                k = NODE_COS;
            } 
            else if (strcmp(t, "tan") == 0) {
                k = NODE_TAN;
            } 
            else if (strcmp(t, "ctg") == 0) {
                k = NODE_CTG;
            } 
            else {
                fprintf(stderr, "unknown unary operator: %s\n", t);
                exit(1);
            }
            stack[sp++] = un(k, a); // добавили на стек новую операцию
        } 
        else // случай если какой то неправильный токен
        {
            fprintf(stderr, "line %d: unknown token %s\n", line_no, t);
            exit(1);
        }
        if (sp >= 4096)  // стек переполнился, слишком длинное выражение
        { 
            fprintf(stderr, "line %d: expression is too long\n", line_no); exit(1); 
        }
        t = strtok(NULL, " \t\r\n"); // получаем след токен
    }

    // sp — это количество элементов, которые остались на стеке после разбора выражения.
    // В правильной обратной польской записи в конце должен остаться ровно один элемент — готовое дерево всей функции.
    if (sp != 1) 
    { 
        fprintf(stderr, "line %d: bad RPN expression\n", line_no); 
        exit(1); 
    }

    // возвращает готовое упрощённое дерево функции
    return simplify(stack[0]);
}


// Нужны, чтобы хранить все числовые константы, которые потом будут выведены в funcs.asm.
// В nasm x86 нельзя напрямую загрузить вещественную константу. Обычно число кладут в секцию данных
// Поэтому генератор собирает все числа в массив constants,
// а потом размещает их в секцию данных .data

static double constants[20000]; //constants — массив чисел типа double.
static int constants_count = 0; //constants_count — сколько чисел уже записано в массив.


static void add_constant(double x){
    if (constants_count >= 20000) 
    { 
        fprintf(stderr, "too many constants\n"); 
        exit(1); 
    }
    constants[constants_count] = x;
    constants_count++;
    return;
}

static void collect_constants(const Node *n) {
    // Эта функция обходит дерево выражения и собирает все числовые константы в общий массив constants.
    // Аргументы: const Node *n - указатель на начало дерева выражения

    if (!n){
        return;
    } 
    if (n->type == NODE_NUM){
        add_constant(n->value);
    }
    collect_constants(n->left);
    collect_constants(n->right);
}

static int same_double(double a, double b) {
    // проверяет примерно равны ли числа
    // Аргументы: double a, double b
    // Функция возвращает int: 
    // 1 - да
    // 0 - нет
    return fabs(a - b) <= 1e-15 * (1.0 + fabs(a) + fabs(b));
}



static int constant_id(double x) {
    // Эта функция выдаёт номер константы в массиве constants.
    // Аргументы: double x - число, id которого нам нужен
    // Функция возвращает: номер константы x.

    for (int i = 0; i < constants_count; ++i) {
        if (same_double(constants[i], x)){
            return i;
        } 
    }
    return 0;
}

static void print_double(FILE *out, double x) {
    // print_double печатает double в funcs.asm так, чтобы NASM понял его как вещественное число.
    // Аргументы: 
    //      out — файл, куда печатаем, например funcs.asm
    //      x   — число, которое надо напечатать

    char buf[128]; // Создаётся временный массив символов, куда сначала будет записано число в виде строки.

    if (x == 0.0) 
    {   
        fputs("0.0", out); 
        return; 
    }
    snprintf(buf, sizeof(buf), "%.17g", x); // превращает число x в текст. 
                                            //%.17g значит напечатать double с точностью до 17 значащих цифр
    fputs(buf, out);
    if (!strchr(buf, '.') && !strchr(buf, 'e') && !strchr(buf, 'E')) { //Это проверка если в числе нет точки и нет экспоненты, добавить .0.
        fputs(".0", out);
    }
}

static int max_int(int a, int b) {
    return a > b ? a : b;
}

static int temp_slots_needed(const Node *n) {
    // Функция смотрит на дерево выражения и считает,
    // какой максимальной глубины временный массив tmp нужен
    // для безопасной генерации x87-кода.
    // Аргументы: указатель на начало дерева
    // Функция возвращает: int - колво временных ячеек tmp

    if (n == NULL) {
        return 0;
    }

    if (n->type == NODE_NUM || n->type == NODE_X) {
        return 0;
    }

    // Унарная операция сама не требует новой tmp-ячейки.
    // Нужно только столько tmp, сколько нужно её аргументу.
    if (n->type == NODE_SIN ||
        n->type == NODE_COS ||
        n->type == NODE_TAN ||
        n->type == NODE_CTG) {
        return temp_slots_needed(n->left);
    }

    // Бинарная операция требует одну tmp-ячейку
    // для сохранения результата одного поддерева.
    if (n->type == NODE_ADD ||
        n->type == NODE_SUB ||
        n->type == NODE_MUL ||
        n->type == NODE_DIV) {

        int left_slots = temp_slots_needed(n->left);
        int right_slots = temp_slots_needed(n->right);

        return 1 + max_int(left_slots, right_slots);
    }

    return 0;
}

//  Предварительное объявление gen_expr.
//  Оно нужно, потому что gen_binary вызывает gen_expr,
//  а сама gen_expr определена ниже. 
static void gen_expr(FILE *out, const Node *n, int depth);

static void gen_binary(FILE *out, const Node *n, int depth, const char *op) {

    // Функция генерирует NASM-код для бинарной операции
    //  Аргументы:
    //       out   — файл funcs.asm, куда печатается NASM-код;
    //       n     — узел дерева, соответствующий бинарной операции;
    //       depth — текущая глубина рекурсии, используется как номер tmp-ячейки;
    //       op    — строка с x87-инструкцией, которую надо вывести.
    // Фукнция ничего не возвращает, она меняет .asm файл

    // Главная проблема:
    // у x87 всего 8 стековых регистров ST0...ST7.
    // Если наивно вычислять большие поддеревья и оставлять все результаты
    // на x87-стеке, стек может переполниться.
    // Поэтому используется временная память tmp

    // При вызове функций подсчета операндов используется depth + 1, 
    // потому что внутри поддерева могут быть свои бинарные операции.
    // Если текущая операция использует: tmp + 0 то вложенная операция должна использовать другую ячейку: tmp + 8
    // Иначе она могла бы перезаписать временное значение родителя.

    gen_expr(out, n->left, depth + 1); // сначала считаем левое поддерево, результат лежит наверху стека
    fprintf(out, "fstp qword [tmp + %d]\n", depth * 8); // сохранили в память результат левого поддерева и убрали его со стека
    gen_expr(out, n->right, depth + 1); // посчитали правое поддерево, результат лежит наверху стека
    fprintf(out, "fld qword [tmp + %d]\n", depth * 8); // загрузили результат из памяти на вершину стека
    fprintf(out, "%s\n", op); // сделали бинарную операцию
                              // операнд из левого поддерева в ST0 операнд из правого поддерева в ST1

    // После выполнения сгенерированного кода на вершине x87-стека ST0
    // остаётся ровно один результат всей бинарной операции.                            
}

static void gen_expr(FILE *out, const Node *n, int depth) {

    //  Функция получает дерево и печатает NASM-код, который вычислит это выражение.

    //  Главный инвариант gen_expr:
    //  после выполнения сгенерированного кода
    //  значение выражения находится в ST0,
    //  а все временные значения, появившиеся во время вычисления,
    //  удалены со стека x87.
    //  То есть gen_expr добавляет на x87-стек ровно одно новое значение —
    //  результат выражения. 


    // Аргументы:
    //     out   — файл funcs.asm, куда печатается NASM-код;
    //     n     — текущий узел дерева выражения;
    //     depth — глубина рекурсии, нужна для выбора ячейки tmp.
    //.    Фукнция ничего не возвращает, она меняет .asm файл


    if (n->type == NODE_NUM) {
        fprintf(out, "fld qword [const%d]\n", constant_id(n->value)); // инструкция делает: ST0 = node->value
    } 
    else if (n->type == NODE_X) {
        fprintf(out, "fld qword [ebp + 8]\n");  // инструкция делает: ST0 = x
    } 
    else if (n->type == NODE_ADD) {
        gen_binary(out, n, depth, "faddp st1, st0");   // инструкция faddp st1, st0 делает: ST1 = ST1 + ST0
                                                        //                                   pop ST0
                                                        //                                   после faddp st1, st0 станет:
                                                        //                                   ST0 = left + right
    } 
    else if (n->type == NODE_SUB) {
        gen_binary(out, n, depth, "fsubrp st1, st0");  // инструкция fsubrp st1, st0 делает: ST1 = ST0 - ST1
                                                        //                                     pop ST0
                                                        //                                     после fsubrp st1, st0 станет:
                                                        //                                     ST0 = left - right
    } 
    else if (n->type == NODE_MUL) {
        gen_binary(out, n, depth, "fmulp st1, st0");   // инструкция fmulp st1, st0  делает: ST1 = ST1 * ST0
                                                        //                                    pop ST0
                                                        //                                    после fmulp st1, st0 станет:
                                                        //                                    ST0 = right * left
    } 
    else if (n->type == NODE_DIV) {
        gen_binary(out, n, depth, "fdivrp st1, st0");  // инструкция fdivrp st1, st0 делает: ST1 = ST0 / ST1
                                                        //                            pop ST0
                                                        //                            после fdivrp st1, st0 станет:
                                                        //                            ST0 = left / right
    } 
    else if (n->type == NODE_SIN) {
        gen_expr(out, n->left, depth); // STO = left
        fprintf(out, "fsin\n"); // STO = sin(left)
    } 
    else if (n->type == NODE_COS) {
        gen_expr(out, n->left, depth); // STO = left
        fprintf(out, "fcos\n");     // STO = cos(left)
    } 
    else if (n->type == NODE_TAN) {
        gen_expr(out, n->left, depth); 
        fprintf(out, "fptan\n");         // после fptan: ST0 = 1.0               
                                         //              ST1 = tan(x)            
        fprintf(out, "fxch st1\n");      // после fxch st1: ST1 и ST0 меняются местами
                                         //                 ST0 = tan(X); ST1 = 1.0
        fprintf(out, "fstp st1\n");      // после fstp st1:
                                         // Инструкция копирует значение из ST(0) в ST(1), а затем делает Pop
                                         // st0 = tan(X)
    }
    else if (n->type == NODE_CTG) {
        gen_expr(out, n->left, depth);
        fprintf(out, "fptan\n");          // После выполнения fptan стек выглядит так:
                                              // ST(0) = 1.0 (новая вершина)
                                              // ST(1) = tan(x)
        fprintf(out, "fdivrp st1, st0\n"); // После fdivrp st1, st0
                                              // ST0 = ctg(x)
    }
}

static void gen_func(FILE *out, const char *name, const Node *expr) {
// Генерирует одну NASM-функцию по дереву выражения.
//     Аргументы:
//        out  — файл, куда записывается NASM-код;
//        name — имя генерируемой функции, например "f1", "f2", "df1";
//        expr — дерево выражения, для которого надо сгенерировать код.
//     Функция ничего не возвращает, только записывает текст ассемблерной функции в файл out.

    fprintf(out, "%s:\n", name); // метка фунции
    fprintf(out, "push ebp\n"); // пролог
    fprintf(out, "mov ebp, esp\n");
    gen_expr(out, expr, 0); // начало рекурсивной генерации функции ! 
    fprintf(out, "leave\n"); // эпилог
    fprintf(out, "ret\n");

}

static void generate_asm(const char *path, Node *f[3], Node *df[3]) {
    // Функция создает и записывает в файл весь ассемблер, вызывая ранее описаные функции
    // Аргументы: 
    //    const char *path - путь к файлу, куда писать ассемблерный код
    //    Node *f[3] - массив из трёх деревьев функций:
    //    Node *df[3] - массив из трех деревьев производных
    //  Функция ничего не возвращает

    int tmp_slots = 1; // tmp_slots — сколько временных ячеек нужно выделить в .bss для массива tmp.
                       // Минимум ставим 1, чтобы даже для простых выражений была хотя бы одна ячейка

    constants_count = 0; // очищает таблицу констант перед генерацией нового funcs.asm.

    // в цикле подсчитываем tmp_slots такой чтобы хватило точно любой функции или производной
    for (int i = 0; i < 3; ++i) {
        collect_constants(f[i]);
        collect_constants(df[i]);
        tmp_slots = max_int(tmp_slots, temp_slots_needed(f[i]));
        tmp_slots = max_int(tmp_slots, temp_slots_needed(df[i]));
    }

    FILE *out = fopen(path, "w"); // открыли файл

    if (!out) {  // ой, вдруг не открылся
        perror(path); exit(1); 
    }

    // объявляем section .data и печатаем туда все константы
    fprintf(out, "section .data\n"); 
    for (int i = 0; i < constants_count; ++i) {
        fprintf(out, "const%d dq ", i); // dq означает define quadword, то есть записать 8 байт. Это как раз размер double.
        print_double(out, constants[i]);
        fprintf(out, "\n");
    }

    // Печать секции .bss. 
    // В неё помещается временный массив, он нужен для gen_binary, где результат левого поддерева временно сохраняется в память
    fprintf(out, "\nsection .bss\n");
    fprintf(out, "tmp resq %d\n", tmp_slots);

    //.text — секция кода. Дальше будут печататься сами функции.
    fprintf(out, "\nsection .text\n");

    // Объявление глобальных функций.  
    // global нужен, чтобы эти функции были видны снаружи, то есть чтобы C-код мог их вызвать:
    fprintf(out, "global f1\nglobal f2\nglobal f3\nglobal df1\nglobal df2\nglobal df3\n\n");

    gen_func(out, "f1", f[0]);
    gen_func(out, "f2", f[1]); 
    gen_func(out, "f3", f[2]);
    gen_func(out, "df1", df[0]); 
    gen_func(out, "df2", df[1]);
    gen_func(out, "df3", df[2]);

    fclose(out);
}

static void generate_header(const char *path, double a, double b) {

    // Эта функция создаёт файл generated_spec.h.
    // Он нужен основной программе main.c, чтобы знать границы отрезка, прочитанные из SPEC_FILE.
    
    // Аргументы:
    // const char *path - путь к заголовочному файлу
    // double a - левая граница отрезка.
    // double b - правая граница отрезка

    FILE *out = fopen(path, "w"); // открыли на запись

    if (!out) { // ой вдруг не открыли
        perror(path); 
        exit(1); 
    }
    fprintf(out, "#ifndef GENERATED_SPEC_H\n");
    fprintf(out, "#define GENERATED_SPEC_H\n\n");
    fprintf(out, "#define SPEC_A "); 
    print_double(out, a); 
    fprintf(out, "\n");
    fprintf(out, "#define SPEC_B "); 
    print_double(out, b); 
    fprintf(out, "\n\n#endif\n");
    fclose(out);
}

static int read_line(FILE *in, char *buf, size_t size, int *line_no) {
//  Читает из файла следующую непустую строку.
//  Параметры:
//     in      — файл, из которого читаем;
//     buf     — буфер, куда записывается строка;
//     size    — размер буфера;
//     line_no — счётчик строк в файле.
//  Что делает:
//      - читает строки через fgets;
//      - увеличивает line_no для каждой прочитанной строки;
//      - пропускает пустые строки и строки только из пробелов/табов;
//      - если нашла непустую строку, записывает её в buf и возвращает 1;
//      - если файл закончился, возвращает 0.

    while (fgets(buf, (int)size, in)) {
        ++*line_no;
        int ok = 0;
        for (char *p = buf; *p; ++p) {
            if (!isspace((unsigned char)*p)) {
                ok = 1;
            }
        }
        if (ok) {
            return 1;
        }
    }
    return 0;
}

int main(int argc, char **argv) {
    // Проверяем количество аргументов командной строки.
    //
    // Должно быть 4 аргумента:
    // argv[0] — имя программы generator;
    // argv[1] — входной файл SPEC_FILE;
    // argv[2] — выходной файл OUT_ASM;
    // argv[3] — выходной файл OUT_HEADER.
    //
    // Пример запуска:
    // ./generator in.txt funcs.asm generated_spec.h
    if (argc != 4) {
        fprintf(stderr, "usage: %s SPEC_FILE OUT_ASM OUT_HEADER\n", argv[0]);
        return 1;
    }

    // Открываем файл со спецификацией функций.
    // В нём первая непустая строка содержит границы A и B,
    // а следующие три непустые строки содержат функции в RPN.
    FILE *in = fopen(argv[1], "r");
    if (!in) {
        perror(argv[1]);
        return 1;
    }

    // Буфер для чтения строк из SPEC_FILE.
    char line[8192];

    // Счётчик строк. Нужен для сообщений об ошибках при разборе выражений.
    int line_no = 0;

    // Границы отрезка, на котором основная программа будет искать корни.
    double a, b;

    // Читаем первую непустую строку и извлекаем из неё два числа: A и B.
    // Если строки нет или в ней не два числа, завершаем генератор с ошибкой.
    if (!read_line(in, line, sizeof(line), &line_no) || sscanf(line, "%lf%lf", &a, &b) != 2) {
        fprintf(stderr, "first line must contain A B\n");
        return 1;
    }

    // f  — деревья выражений для функций f1, f2, f3.
    // df — деревья выражений для производных df1, df2, df3.
    Node *f[3], *df[3];

    // Читаем три следующие непустые строки.
    // Каждая строка описывает одну функцию в обратной польской записи.
    for (int i = 0; i < 3; ++i) {
        // Если строк меньше трёх, входной файл имеет неправильный формат.
        if (!read_line(in, line, sizeof(line), &line_no)) {
            fprintf(stderr, "expected three RPN expressions\n");
            return 1;
        }

        // Строим дерево выражения для очередной функции.
        f[i] = parse_funcstrings(line, line_no);

        // Строим дерево производной и упрощаем его.
        df[i] = simplify(derivative(f[i]));
    }

    // После чтения всех данных входной файл больше не нужен.
    fclose(in);

    // Генерируем NASM-файл с функциями f1, f2, f3
    // и производными df1, df2, df3.
    generate_asm(argv[2], f, df);

    // Генерируем C-заголовок generated_spec.h
    // с макросами SPEC_A и SPEC_B.
    generate_header(argv[3], a, b);

    // Успешное завершение генератора.
    return 0;
}