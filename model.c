#include "model.h"
#include "interface.h"

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//Maximum number of operands in a formula.
#define MAX_TERMS 10

/*
 * Enum to represent the type of cell in the spreadsheet.
 * TEXT: Cell containing a text string.
 * NUMBER: Call containing a number.
 * FORMULA: Cell containing a formula.
 * */
typedef enum {
    TEXT,
    NUMBER,
    FORMULA
} CellType;

//Declaration of the cell struct.
typedef struct Cell Cell;

/*
 * Struct to represent the terms in a formula.
 * cell: Pointer to the cell that the term points to. ie, A1, B2, etc.
 * constant: The constant value term. ie, 1.4, 2.9, etc.
 */
typedef struct {
    Cell* cell;
    double constant;
} Terms;

/*
 * Struct to represent a formula.
 * Terms: Array of terms in the formula.
 * num_terms: Number of terms in the formula.
 */
typedef struct {
    Terms terms[MAX_TERMS];
    int num_terms;
} Formula;

/*
 * Union to represent the value of a cell.
 * text: Text of the cell.
 * number: Number of cell.
 * formula: Formula of cell.
 */
typedef union {
    char* text;
    double number;
    Formula* formula;
} CellValue;

/*
 * Enum to represent the evaluation state of a cell. Used to determine circular dependencies.
 * EVALUATING: Cell is currently being evaluated.
 * EVALUATED: Cell has been evaluated.
 */
typedef enum {
    NOT_EVALUATED,
    EVALUATING,
    EVALUATED
} EvaluationState;

/*
 * Struct to represent a cell in the spreadsheet.
 * type: Type of cell. ie, text, number, formula.
 * value: Value of cell. ie, text, number, formula.
 * EvalState: Evaluation state of cell. ie, evaluating or evaluated.
 */
struct Cell {
    CellType type;
    CellValue value;
    EvaluationState state;
};

/*
 * 2D array of cells to represent the spreadsheet.
 * Cell is a struct which contains the type, value, and evaluation state of the cell.
 * NUM_ROWS: Number of rows in the spreadsheet.
 * NUM_COLS: Number of columns in the spreadsheet.
 */
Cell spreadsheet[NUM_ROWS][NUM_COLS];

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/*
 * Retrieve a cell from the spreadsheet by its reference.
 * reference: Reference of the cell. ie, A1, B2, etc.
 * Return: Pointer to the cell.
 */
Cell* get_cell_by_reference(char* reference) {
    //Checks if the reference is NULL or if the length of the reference is less than 2.
    if (reference == NULL || strlen(reference) < 2) {
        return NULL;
    }

    //Checks if the first character of the reference is a letter.
    if (reference[0] >= 'a' && reference[0] <= 'z') {
        return NULL;
    }
    //Determines the column index by subtracting 'A' from the first character of the reference.
    COL column = reference[0] - 'A';

    //Determines the row index by converting the string to an integer.
    char* end;
    long row = strtol(reference + 1, &end, 10);
    //Checks if row conversion was unsuccessful or if the row is out of bounds.
    if (*end != '\0' || row < 1 || row > NUM_ROWS) {
        return NULL;
    }

    //Subtract 1 because row numbers start from 1 but array indices start from 0
    row -= 1;

    //Returns a pointer to the cell.
    return &spreadsheet[row][column];
}

/*
 * Retrieve a cell from the spreadsheet by its row and column.
 * cell: cell which will be found
 * Return: Pointer to the cell.
 */
char* get_cell_reference(Cell* cell) {
    //Determines the row and column of the cell.
    int row = (cell - &spreadsheet[0][0]) / NUM_COLS;
    int col = (cell - &spreadsheet[0][0]) % NUM_COLS;

    //Creates string to store reference
    char* reference = malloc(4 * sizeof(char));
    //Checks if the reference is NULL.
    if (reference == NULL) {
        return NULL;
    }

    //Adds the column and row to the reference.
    sprintf(reference, "%c%d", col + 'A', row + 1);

    //Returns string containing the cell reference
    return reference;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/*
 * Converts a string to a formula.
 * text: String to be converted.
 * return: Pointer to the formula.
 */
Formula* string_to_formula(char* text) {
    //Creates a formula struct and sets the size of formula to 0.
    Formula* formula = malloc(sizeof(Formula));
    formula->num_terms = 0;

    //Splits the string between the '+' term to get the terms of the formula.
    char* split_term = strtok(text + 1, "+");
    while (split_term != NULL) {
        Terms term;

        char* end;
        //Tries to convert the term into a number.
        double number = strtod(split_term, &end);

        //If the conversion to a number is successful, then the term is a constant.
        if (end != split_term) {
            term.constant = number;
            term.cell = NULL;
        }
        //If the conversion to a number is unsuccessful, then the term is a cell reference and then cell is retrieved.
        else {
            term.cell = get_cell_by_reference(split_term);
            if (term.cell == NULL) {
                free(formula);
                return NULL;
            }
        }
        //Adds the term to the formula array.
        formula->terms[formula->num_terms++] = term;
        split_term = strtok(NULL, "+");
    }
    //Returns the formula.
    return formula;
}

/*
 * Evaluates a formula and determines the answer.
 * formula: Formula to be evaluated.
 * return: Result of the formula.
 */
double evaluate_formula(Formula* formula) {
    //Initializes the answer to 0.
    double answer = 0.0;

    //Iterates over each term in the formula.
    for (int i = 0; i < formula->num_terms; i++) {
        //Retrieves the term from the formula array which stores terms.
        Terms term = formula->terms[i];

        //Checks if the term is a cell reference
        if (term.cell != NULL) {
            //Checks if the cell is currently being evaluated. If true, then a circular dependency has been detected.
            if (term.cell->state == EVALUATING) {
                //Returns NAN as a error code to report a circular dependency has occurred.
                return NAN;
            }
            //If the cell contains a formula, it sets the cell's state to evaluating and recursively evaluates the formula.
            else if (term.cell->type == FORMULA) {
                term.cell->state = EVALUATING;
                double formula_result = evaluate_formula(term.cell->value.formula);
                //Checks if an error has occurred while evaluating the formula in the cell.
                if (isinf(formula_result)) {
                    //Returns INFINITY as an error code to report that the cell contains a non-numeric value.
                    return INFINITY;
                }
                //Adds the result of the formula within the cell to the answer.
                answer += formula_result;
                //Changes the state of the cell to "evaluated".
                term.cell->state = EVALUATED;
            }
            //If the cell contains a number, it sets the cell's state to evaluating and adds the number to the answer.
            else if (term.cell->type == NUMBER) {
                term.cell->state = EVALUATING;
                //Adds the number to the answer
                answer += term.cell->value.number;
                //Changes the state of the cell to "evaluated".
                term.cell->state = EVALUATED;
            } else {
                //Returns INFINITY as an error code to report that an error has occurred during the evaluation process.
                return INFINITY;
            }
        }
        //Adds the constant to the answer
        else {
            answer += term.constant;
        }
    }
    //Returns the answer.
    return answer;
}

/*
 * Converts a formula to a string.
 * formula: The formula which is to be converted to string.
 * return: String which contains the formula.
 */
char* formula_to_string(Formula* formula) {
    //Allocates memory for the string which will contain the formula and a null terminator.
    char* text = malloc(CELL_DISPLAY_WIDTH + 1);

    //Checks if the memory allocation was successful.
    if (text == NULL) {
        return NULL;
    }

    //Sets the first char to '=' and the last char to the null terminator.
    text[0] = '=';
    text[1] = '\0';

    for (int i = 0; i < formula->num_terms; i++) {
        //Retrieves the term from the array of terms in the formula.
        Terms term = formula->terms[i];

        //Checks if the term is a cell reference or constant.
        if (term.cell != NULL) {
            //If the term is a cell reference, it retrieves the cell reference as a string.
            char* cell_reference = get_cell_reference(term.cell);
            //Appends the cell reference string to the string containing the formula
            strcat(text, cell_reference);
            //Frees the memory allocated for the cell reference string.
            free(cell_reference);
        }
            //If the term is a constant, it adds the constant to the string.
        else {
            char number[20];
            //Converts the constant to a string.
            snprintf(number, 20, "%f", term.constant);
            //Appends the constant string to the string containing the formula.
            strcat(text, number);
        }
        //Adds the addition symbol '+' to the string if there are more terms, and it's not the last term.
        if (i < formula->num_terms - 1) {
            //Appends the addition symbol
            strcat(text, "+");
        }
    }
    //Returns the string.
    return text;
}

/*
 * Frees the memory allocated for a formula.
 * formula: Formula to be freed.
 */
void free_formula(Formula* formula) {
    if (formula != NULL) {
        free(formula);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/*
 * Initializes the 2D array which contains the spreadsheet.
 */
void model_init() {
    // TODO: implement this.
    for (int i = 0; i < NUM_ROWS; i++) {
        for (int j = 0; j < NUM_COLS; j++) {
            //Sets the type of the cells in the spreadsheet to TEXT.
            spreadsheet[i][j].type = TEXT;
            //Sets the value of the cells in the spreadsheet to NULL.
            spreadsheet[i][j].value.text = NULL;
        }
    }
}


/*
 * Sets the value of a cell based on user input.
 * text: String which contains the user input.
 */
void set_cell_value(ROW row, COL col, char *text) {
    //Checks if the cell which is being changed contains a formula.
    if (spreadsheet[row][col].type == FORMULA) {
        //Frees the memory allocated for the formula.
        free_formula(spreadsheet[row][col].value.formula);
    }

    //Checks if the user is attempting to create a formula.
    if (text[0] == '=') {
        //Changes the cell type to FORMULA.
        spreadsheet[row][col].type = FORMULA;
        //Converts the string to a formula.
        spreadsheet[row][col].value.formula = string_to_formula(text);

        //Checks if the formula conversion was unsuccessful due to invalid syntax.
        if (spreadsheet[row][col].value.formula == NULL) {
            //Prints an error message to the user through the cell text.
            update_cell_display(row, col, "Error: Failed to parse formula");
        }

        //Evaluates the formula and determines the answer.
        double result = evaluate_formula(spreadsheet[row][col].value.formula);
        //Checks for error code relating to circular dependency.
        if (isnan(result)) {
            //Prints an error message telling the user that a circular dependency is occurring in the cell which is being used.
            update_cell_display(row, col, "Error: circular dependency detected");
        }
            //Checks if an error has occurred due to a cell containing a non-numeric value.
        else if (isinf(result)) {
            //Prints an error message telling the user that a cell contains a non-numeric value.
            update_cell_display(row, col, "Error: cell contains non-numeric value");
        } else {
            //Converts the result to string.
            char result_str[CELL_DISPLAY_WIDTH + 1];
            snprintf(result_str, CELL_DISPLAY_WIDTH + 1, "%f", result);
            //Updates the cell display with the resulting string.
            update_cell_display(row, col, result_str);
        }
    }
        //Tries converting the input text to a number.
    else {
        char* end;
        double number = strtod(text, &end);

        //Checks if the conversion to a number was successful.
        if (end != text) {
            //Changes the cell type to NUMBER.
            spreadsheet[row][col].type = NUMBER;
            //Sets the cell value to the number.
            spreadsheet[row][col].value.number = number;
            //Updates the cell display with the number.
            update_cell_display(row, col, text);
        }
            //If the conversion failed, then the input text is a string
        else {
            //Changes the cell type to TEXT.
            spreadsheet[row][col].type = TEXT;
            //Sets the cell value to the string.
            spreadsheet[row][col].value.text = strdup(text);
            //Updates the cell display with the string.
            update_cell_display(row, col, text);
        }
    }
    //Frees the string containing the user input.
    free(text);
}

/*
 * Clears the value of a cell.
 * row: Row of the cell which is going to be cleared.
 * col: Column of the cell which is going to be cleared.
 */
void clear_cell(ROW row, COL col) {
    // TODO: implement this.
    //Checks if the cell contains a formula.
    if (spreadsheet[row][col].type == FORMULA) {
        //Frees the memory allocated for the formula.
        free_formula(spreadsheet[row][col].value.formula);
    }
        //Checks if the cell contains a string and isn't already empty.
    else if (spreadsheet[row][col].type == TEXT && spreadsheet[row][col].value.text != NULL) {
        //Frees the memory allocated for the cell text string.
        free(spreadsheet[row][col].value.text);
    }

    //Sets the cell type to TEXT.
    spreadsheet[row][col].type = TEXT;
    //Sets the cell value to NULL.
    spreadsheet[row][col].value.text = NULL;

    //Updates the cell display with an empty string.
    update_cell_display(row, col, "");
}

/*
 * Creates a textual version of a cell for editing.
 * row: Row of the cell which is going to be retrieved.
 * col: Column of the cell which is going to be retrieved.
 * return: String which contains the cell value.
 */
char *get_textual_value(ROW row, COL col) {
    //Creates a string which will contain the cell value.
    char *textual_value;

    //Checks if the cell contains a formula.
    if (spreadsheet[row][col].type == FORMULA) {
        //Converts the formula to a string and stores it in the string.
        textual_value = formula_to_string(spreadsheet[row][col].value.formula);
    }
        //Checks if the cell contains a number.
    else if (spreadsheet[row][col].type == NUMBER) {
        //Allocates memory for a string which contains the number.
        textual_value = malloc(CELL_DISPLAY_WIDTH + 1);
        //Adds the number to the string.
        snprintf(textual_value, CELL_DISPLAY_WIDTH + 1, "%f", spreadsheet[row][col].value.number);
    }
        //If the cell contains a string, then it copies the string to the textual string.
    else {
        //Checks if the cell is empty.
        if (spreadsheet[row][col].value.text != NULL) {
            //Copies the cell string to the textual string.
            textual_value = strdup(spreadsheet[row][col].value.text);
        }
            //Executes if the cell is empty.
        else {
            //Allocate memory for an empty string
            textual_value = malloc(1);
            //Set the first character to the null terminator
            textual_value[0] = '\0';
        }
    }
    //Returns the textual string.
    return textual_value;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////