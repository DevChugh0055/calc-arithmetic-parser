// Devansh Chugh 241ADB054
// https://github.com/DevChugh0055/calc-arithmetic-parser
// Compile with: gcc -O2 -Wall -Wextra -std=c17 -o calc calc.c -lm

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <errno.h>
#include <dirent.h>
#include <sys/stat.h>

// FIX: Include direct.h for _mkdir on Windows
#ifdef _WIN32
#include <direct.h>
#endif

#define MAX_INPUT_SIZE 10000
#define MAX_FILENAME 256

typedef enum {
    TOK_NUMBER,
    TOK_PLUS,
    TOK_MINUS,
    TOK_STAR,
    TOK_SLASH,
    TOK_POW,
    TOK_LPAREN,
    TOK_RPAREN,
    TOK_EOF,
    TOK_ERROR
} TokenType;

typedef struct {
    TokenType type;
    double value;      // For numbers
    int start_pos;     // 1-based starting position in input
    int length;        // Token length
} Token;

typedef struct {
    const char *input;
    int pos;           // Current position in input (0-based)
    int length;        // Total input length
    Token current_token;
    int has_error;
    int error_pos;
    const char *error_msg;
} Parser;

// Function declarations
void init_parser(Parser *parser, const char *input, int length);
Token get_next_token(Parser *parser);
double parse_expression(Parser *parser);
double parse_term(Parser *parser);
double parse_factor(Parser *parser);
double parse_power(Parser *parser);
double parse_primary(Parser *parser);
void skip_whitespace(Parser *parser);
int is_comment_line(const char *line);
void process_file(const char *input_path, const char *output_dir,
                  const char *name, const char *lastname, const char *studentid);
void process_directory(const char *input_dir, const char *output_dir,
                       const char *name, const char *lastname, const char *studentid);
char* get_student_info(char *name, char *lastname, char *studentid);

void init_parser(Parser *parser, const char *input, int length) {
    parser->input = input;
    parser->pos = 0;
    parser->length = length;
    parser->has_error = 0;
    parser->error_pos = -1;
    parser->error_msg = NULL;
    parser->current_token = get_next_token(parser);
}

void skip_whitespace(Parser *parser) {
    while (parser->pos < parser->length && isspace(parser->input[parser->pos])) {
        parser->pos++;
    }
}

Token get_next_token(Parser *parser) {
    Token token;
    
    skip_whitespace(parser);
    
    token.start_pos = parser->pos + 1; // Convert to 1-based *after* skipping whitespace

    if (parser->pos >= parser->length) {
        token.type = TOK_EOF;
        token.length = 0;
        return token;
    }

    char current = parser->input[parser->pos];

    // Handle numbers (integers and floats)
    if (isdigit(current) || current == '.') {
        char *end;
        // Set errno to 0 before call
        errno = 0;
        double value = strtod(parser->input + parser->pos, &end);

        if (errno == ERANGE) {
            // Handle overflow or underflow
            token.type = TOK_ERROR;
            token.length = end - (parser->input + parser->pos);
            parser->pos += token.length;
        } else if (end == parser->input + parser->pos) {
            // This case should be rare if starting with '.' or digit, 
            // but handles just a single '.'
            token.type = TOK_ERROR;
            token.length = 1;
            parser->pos++;
        } else {
            token.type = TOK_NUMBER;
            token.value = value;
            token.length = end - (parser->input + parser->pos);
            parser->pos += token.length;
        }
        return token;
    }

    // Handle operators
    switch (current) {
        case '+':
            token.type = TOK_PLUS;
            token.length = 1;
            parser->pos++;
            break;
        case '-':
            token.type = TOK_MINUS;
            token.length = 1;
            parser->pos++;
            break;
        case '*':
            if (parser->pos + 1 < parser->length && parser->input[parser->pos + 1] == '*') {
                token.type = TOK_POW;
                token.length = 2;
                parser->pos += 2;
            } else {
                token.type = TOK_STAR;
                token.length = 1;
                parser->pos++;
            }
            break;
        case '/':
            token.type = TOK_SLASH;
            token.length = 1;
            parser->pos++;
            break;
        case '(':
            token.type = TOK_LPAREN;
            token.length = 1;
            parser->pos++;
            break;
        case ')':
            token.type = TOK_RPAREN;
            token.length = 1;
            parser->pos++;
            break;
        default:
            token.type = TOK_ERROR;
            token.length = 1;
            parser->pos++;
            break;
    }

    return token;
}

double parse_primary(Parser *parser) {
    Token token = parser->current_token;

    if (token.type == TOK_NUMBER) {
        parser->current_token = get_next_token(parser);
        return token.value;
    }

    if (token.type == TOK_LPAREN) {
        parser->current_token = get_next_token(parser);
        double result = parse_expression(parser);

        if (parser->has_error) return 0;

        if (parser->current_token.type != TOK_RPAREN) {
            parser->has_error = 1;
            parser->error_pos = parser->current_token.start_pos;
            parser->error_msg = "Expected ')'";
            return 0;
        }

        parser->current_token = get_next_token(parser);
        return result;
    }
    
    parser->has_error = 1;
    parser->error_pos = token.start_pos;
    if (token.type == TOK_ERROR) {
         parser->error_msg = "Invalid character";
    } else {
         parser->error_msg = "Expected number or '('";
    }
    return 0;
}

double parse_power(Parser *parser) {
    double left = parse_primary(parser);

    if (parser->has_error) return 0;

    while (parser->current_token.type == TOK_POW) {
        Token op_token = parser->current_token;
        parser->current_token = get_next_token(parser);

        double right = parse_factor(parser); // Power is right-associative
        if (parser->has_error) return 0;

        left = pow(left, right);
    }

    return left;
}

double parse_factor(Parser *parser) {
    Token token = parser->current_token;

    if (token.type == TOK_MINUS) {
        parser->current_token = get_next_token(parser); // Consume '-'
        double right = parse_factor(parser); // Recursively call
        if (parser->has_error) return 0;
        return -right;
    }
    
    if (token.type == TOK_PLUS) {
        parser->current_token = get_next_token(parser); // Consume '+'
        return parse_factor(parser); // Recursively call
    }

    return parse_power(parser);
}

double parse_term(Parser *parser) {
    double left = parse_factor(parser);

    if (parser->has_error) return 0;

    while (parser->current_token.type == TOK_STAR ||
           parser->current_token.type == TOK_SLASH) {
        Token op_token = parser->current_token;
        parser->current_token = get_next_token(parser);

        double right = parse_factor(parser);
        if (parser->has_error) return 0;

        if (op_token.type == TOK_STAR) {
            left *= right;
        } else { // TOK_SLASH
            if (right == 0.0) {
                parser->has_error = 1;
                parser->error_pos = op_token.start_pos;
                parser->error_msg = "Division by zero";
                return 0;
            }
            left /= right;
        }
    }

    return left;
}

double parse_expression(Parser *parser) {
    double left = parse_term(parser);

    if (parser->has_error) return 0;

    while (parser->current_token.type == TOK_PLUS ||
           parser->current_token.type == TOK_MINUS) {
        Token op_token = parser->current_token;
        parser->current_token = get_next_token(parser);

        double right = parse_term(parser);
        if (parser->has_error) return 0;

        if (op_token.type == TOK_PLUS) {
            left += right;
        } else {
            left -= right;
        }
    }

    return left;
}

int is_comment_line(const char *line) {
    while (*line && isspace((unsigned char)*line)) {
        line++;
    }
    return *line == '#';
}

char* get_student_info(char *name, char *lastname, char *studentid) {
    // Using your actual information
    strcpy(name, "Devansh");
    strcpy(lastname, "Chugh");
    strcpy(studentid, "241ADB054");
    return "Devansh Chugh 241ADB054"; 
}

void process_file(const char *input_path, const char *output_dir,
                  const char *name, const char *lastname, const char *studentid) {
    FILE *input_file = fopen(input_path, "r");
    if (!input_file) {
        fprintf(stderr, "Error: Cannot open input file %s\n", input_path);
        return;
    }

    char file_content[MAX_INPUT_SIZE + 1];
    char line_buffer[MAX_INPUT_SIZE + 1]; 
    int content_len = 0;
    file_content[0] = '\0'; 

    while (fgets(line_buffer, sizeof(line_buffer), input_file)) {
        if (!is_comment_line(line_buffer)) {
            if (content_len + strlen(line_buffer) < MAX_INPUT_SIZE) {
                strcat(file_content, line_buffer);
                content_len += strlen(line_buffer);
            } else {
                fprintf(stderr, "Warning: File %s exceeds max size, truncating.\n", input_path);
                break; 
            }
        }
    }
    fclose(input_file);

    if (content_len == 0) {
        return;
    }

    Parser parser;
    init_parser(&parser, file_content, content_len);

    double result = parse_expression(&parser);

    if (!parser.has_error && parser.current_token.type != TOK_EOF) {
        parser.has_error = 1;
        parser.error_pos = parser.current_token.start_pos;
        parser.error_msg = "Unexpected token";
    }

    char output_path[MAX_FILENAME];
    const char *input_filename = strrchr(input_path, '/');
    if (input_filename) {
        input_filename++;
    } else {
        input_filename = input_path;
    }

    char base_name[MAX_FILENAME];
    strcpy(base_name, input_filename);
    char *dot = strrchr(base_name, '.');
    if (dot && strcmp(dot, ".txt") == 0) {
        *dot = '\0';
    }

    snprintf(output_path, sizeof(output_path), "%s/%s_%s_%s_%s.txt",
             output_dir, base_name, name, lastname, studentid);

    FILE *output_file = fopen(output_path, "w");
    if (!output_file) {
        fprintf(stderr, "Error: Cannot create output file %s\n", output_path);
        return;
    }

    if (parser.has_error) {
        fprintf(output_file, "ERROR:%d\n", parser.error_pos);
    } else {
        if (fabs(result - round(result)) < 1e-12) {
            fprintf(output_file, "%.0f\n", result);
        } else {
            fprintf(output_file, "%.15g\n", result);
        }
    }

    fclose(output_file);
}

void process_directory(const char *input_dir, const char *output_dir,
                       const char *name, const char *lastname, const char *studentid) {
    
    DIR *dir = opendir(input_dir);
    if (!dir) {
        fprintf(stderr, "Error: Cannot open directory %s\n", input_dir);
        return;
    }

    struct stat st = {0};
    if (stat(output_dir, &st) == -1) {
        // FIX: Use _mkdir on Windows, mkdir on POSIX
#ifdef _WIN32
        if (_mkdir(output_dir) == -1) {
#else
        if (mkdir(output_dir, 0755) == -1) {
#endif
            fprintf(stderr, "Error: Cannot create output directory %s\n", output_dir);
            closedir(dir);
            return;
        }
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        const char *filename = entry->d_name;
        const char *dot = strrchr(filename, '.');

        if (dot && strcmp(dot, ".txt") == 0) {
            char full_path[MAX_FILENAME];
            snprintf(full_path, sizeof(full_path), "%s/%s", input_dir, filename);
            process_file(full_path, output_dir, name, lastname, studentid);
        }
    } 

    closedir(dir);
}

int main(int argc, char *argv[]) {
    char name[50], lastname[50], studentid[50];
    get_student_info(name, lastname, studentid);

    const char *input_path = NULL;
    const char *input_dir = NULL;
    const char *output_dir = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-d") == 0 || strcmp(argv[i], "--dir") == 0) {
            if (i + 1 < argc) {
                input_dir = argv[++i];
            } else {
                fprintf(stderr, "Error: %s requires a directory argument\n", argv[i]);
                return 1;
            }
        } else if (strcmp(argv[i], "-o") == 0 || strcmp(argv[i], "--output-dir") == 0) {
            if (i + 1 < argc) {
                output_dir = argv[++i];
            } else {
                fprintf(stderr, "Error: %s requires a directory argument\n", argv[i]);
                return 1;
            }
        } else {
            if (input_path) {
                fprintf(stderr, "Error: Cannot specify multiple input files.\n");
                return 1;
            }
            input_path = argv[i];
        }
    }

    if (input_path && input_dir) {
        fprintf(stderr, "Error: Cannot specify both input file and input directory\n");
        return 1;
    }

    if (!input_path && !input_dir) {
        fprintf(stderr, "Error: No input file or directory specified\n");
        fprintf(stderr, "Usage: %s [input.txt] [-d DIR] [-o OUTDIR]\n", argv[0]);
        return 1;
    }

    char default_output_dir[MAX_FILENAME];
    if (!output_dir) {
        if (input_path) {
            const char *base = strrchr(input_path, '/');
            if (base) base++;
            else base = input_path;

            char base_name[MAX_FILENAME];
            strcpy(base_name, base);
            char *dot = strrchr(base_name, '.');
            if (dot) *dot = '\0';

            snprintf(default_output_dir, sizeof(default_output_dir), "%s_%s_%s",
                     base_name, name, studentid);
        } else if (input_dir) {
            const char *base = strrchr(input_dir, '/');
            if (base) base++;
            else base = input_dir;
            
            char dir_name[MAX_FILENAME];
            strcpy(dir_name, base);
            if (dir_name[strlen(dir_name) - 1] == '/') {
                dir_name[strlen(dir_name) - 1] = '\0';
            }
            if (strlen(dir_name) == 0) {
                strcpy(dir_name, "input"); 
            }

            snprintf(default_output_dir, sizeof(default_output_dir), "%s_%s_%s",
                     dir_name, name, studentid);
        }
        output_dir = default_output_dir;
    }
    
    if (input_path) {
         struct stat st = {0};
         if (stat(output_dir, &st) == -1) {
            // FIX: Use _mkdir on Windows, mkdir on POSIX
#ifdef _WIN32
            if (_mkdir(output_dir) == -1) {
#else
            if (mkdir(output_dir, 0755) == -1) {
#endif
                 fprintf(stderr, "Error: Cannot create output directory %s\n", output_dir);
                 return 1;
            }
         }
    }

    if (input_dir) {
        process_directory(input_dir, output_dir, name, lastname, studentid);
    } else {
        process_file(input_path, output_dir, name, lastname, studentid);
    }

    return 0;
}

