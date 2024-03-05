/*
   @title   Arduino Real Time Interpreter (ARTI)
   @file    arti.h
   @date    20220818
   @author  Ewoud Wijma
   @Copyright (c) 2024 Ewoud Wijma
   @repo    https://github.com/ewoudwijma/ARTI
   @remarks
          - #define ARDUINOJSON_DEFAULT_NESTING_LIMIT 100 //set this in ArduinoJson!!!, currently not necessary...
          - IF UPDATING THIS FILE IN THE WLED REPO, SEND A PULL REQUEST TO https://github.com/ewoudwijma/ARTI AS WELL!!!
   @later
          - Code improvement
            - See 'for some weird reason this causes a crash on esp32'
            - check why column/lineno not correct
          - Definition improvements
            - support string (e.g. for print)
            - add integer and string stacks
            - print every x seconds (to use it in loops. e.g. to show free memory)
            - reserved words (ext functions and variables cannot be used as variables)
            - check on return values
            - arrays (indices) for varref
          - WLED improvements
            - rename program to sketch?
   @progress
          - SetPixelColor without colorwheel
          - extend errorOccurred and add warnings (continue) next to errors (stop). Include stack full/empty
          - WLED: *arti in SEGENV.data: not working well as change mode will free(data)
          - move code from interpreter to analyzer to speed up interpreting
   @done?
   @done
          - save log after first run of loop to get runtime errors included (or 30 iterations to also capture any stack overflows)
   @todo
          - check why statement is not 'shrinked'
          - make default work in js (remove default as we have now load template)
          - add PI
          - color_fade_pulse because /pixelCount instead of ledCount should not crash
  */

#pragma once

#define ARTI_SERIAL 1
#define ARTI_FILE 2

#if ARTI_PLATFORM == ARTI_ARDUINO //defined in arti_definition.h e.g. arti_wled.h!
  #include "../../wled00/wled.h"  
  #include "../../wled00/src/dependencies/json/ArduinoJson-v6.h"

  File logFile;

  #define ARTI_ERRORWARNING 1 //shows lexer, parser, analyzer and interpreter errors
  // #define ARTI_DEBUG 1
  // #define ARTI_ANDBG 1
  // #define ARTI_RUNLOG 1 //if set on arduino this will create massive amounts of output (as ran in a loop)
  #define ARTI_MEMORY 1 //to do analyses of memory usage, trace memoryleaks (works only on arduino)
  #define ARTI_PRINT 1 //will show the printf calls

  const char spaces[51] PROGMEM = "                                                  ";
  #if defined(ARDUINO_ARCH_ESP32)
    #define FREE_SIZE esp_get_free_heap_size()
    #define asChar(x) x.as<std::string>().c_str() //WLEDMM: tbd: find out why char * is causing crashes!!!
  #else
    #define FREE_SIZE ESP.getFreeHeap()
    #define asChar(x) ""
    //x.as<const char *>() //WLEDMM: no arduinojson output for the moment as this is in testing phase
  #endif
  // #define OPTIMIZED_TREE 1
#else //embedded
  #include "dependencies/ArduinoJson-recent.h"

  FILE * logFile; // FILE needed to use in fprintf (std stream does not work)

  #define ARTI_ERRORWARNING 1
  #define ARTI_DEBUG 1
  #define ARTI_ANDBG 1
  #define ARTI_RUNLOG 1
  #define ARTI_MEMORY 1
  #define ARTI_PRINT 1

  #include <math.h>
  #include <iostream>
  #include <fstream>
  #include <sstream>

  const char spaces[51]         = "                                                  ";
  #define FREE_SIZE (unsigned int)0 //No free heap function found on embedded
  #define asChar(x) x.as<const char *>()
  // #define OPTIMIZED_TREE 1
#endif

bool logToFile = true; //print output to file (e.g. default.wled.log)
uint32_t frameCounter = 0; //tbd move to class if more instances run 

void artiPrintf(char const * format, ...)
{
  va_list argp;

  va_start(argp, format);

  // if (!logToFile)
  // {
  //   //WLEDMM: not working to use USER_PRINTF here in case of arduino so moving to rocket science part
  //   vprintf(format, argp);
  // }
  // else
  #if ARTI_PLATFORM == ARTI_ARDUINO
    // rocket science here! As logfile.printf causes crashes/wrong output we create our own printf here
    // logFile.printf(format, argp);
    for (size_t i = 0; i < strlen(format); i++) 
    {
      if (format[i] == '%') 
      {
        switch (format[i+1]) 
        {
          case 's':
            if (logToFile) logFile.print(va_arg(argp, const char *)); else USER_PRINT(va_arg(argp, const char *));
            break;
          case 'u':
            if (logToFile) logFile.print(va_arg(argp, unsigned int)); else USER_PRINT(va_arg(argp, unsigned int));
            break;
          case 'c':
            if (logToFile) logFile.print((char)va_arg(argp, int)); else USER_PRINT(va_arg(argp, int));
            break;
          case 'f':
            if (logToFile) logFile.print(va_arg(argp, double)); else USER_PRINT(va_arg(argp, double));
            break;
          case '%':
            if (logToFile) logFile.print("%");  else USER_PRINT("%"); // in case of %%
            break;
          default:
            va_arg(argp, int);
          // logFile.print(x);
            if (logToFile) logFile.print(format[i]); else USER_PRINT(format[i]);
            if (logToFile) logFile.print(format[i+1]); else USER_PRINT(format[i+1]);
        }
        i++;
      } 
      else 
      {
        if (logToFile) logFile.print(format[i]); else USER_PRINT(format[i]);
      }
    }
  #else
    vfprintf(logFile, format, argp);
  #endif
  va_end(argp);
}

//add millis function for non arduino
#if ARTI_PLATFORM != ARTI_ARDUINO
  uint32_t millis()
  {
    return std::chrono::system_clock::now().time_since_epoch().count();
  }
#endif

#ifdef ARTI_DEBUG
    #define DEBUG_ARTI(...) artiPrintf(__VA_ARGS__)
#else
    #define DEBUG_ARTI(...)
#endif

#ifdef ARTI_ANDBG
    #define ANDBG_ARTI(...) artiPrintf(__VA_ARGS__)
#else
    #define ANDBG_ARTI(...)
#endif

#ifdef ARTI_RUNLOG
  #define RUNLOG_ARTI(...) artiPrintf(__VA_ARGS__)
#else
    #define RUNLOG_ARTI(...)
#endif

#ifdef ARTI_PRINT
    #define PRINT_ARTI(...) artiPrintf(__VA_ARGS__)
#else
    #define PRINT_ARTI(...)
#endif

#ifdef ARTI_ERRORWARNING
    #define ERROR_ARTI(...) artiPrintf(__VA_ARGS__)
    #define WARNING_ARTI(...) artiPrintf(__VA_ARGS__)
#else
    #define ERROR_ARTI(...)
    #define WARNING_ARTI(...)
#endif

#ifdef ARTI_MEMORY
    #define MEMORY_ARTI(...) artiPrintf(__VA_ARGS__)
#else
    #define MEMORY_ARTI(...)
#endif

#define charLength 30
#define fileNameLength 50
#define arrayLength 30

#define floatNull -32768

const char * stringOrEmpty(const char *charS)  {
  if (charS == nullptr)
    return "";
  else
    return charS;
}

//define strupr as only supported in windows toolchain
char* strupr(char* s)
{
    char* tmp = s;

    for (;*tmp;++tmp) {
        *tmp = toupper((unsigned char) *tmp);
    }

    return s;
}

enum Tokens
{
  F_integerConstant,
  F_realConstant,
  F_plus,
  F_minus,
  F_multiplication,
  F_division,
  F_modulo,
  F_bitShiftLeft,
  F_bitShiftRight,
  F_equal,
  F_notEqual,
  F_lessThen,
  F_lessThenOrEqual,
  F_greaterThen,
  F_greaterThenOrEqual,
  F_and,
  F_or,
  F_plusplus,
  F_minmin,
  F_NoToken = 255
};

const char * tokenToString(uint8_t key)
{
  switch (key) {
  case F_integerConstant:
    return "integer";
    break;
  case F_realConstant:
    return "real";
    break;
  case F_plus:
    return "+";
    break;
  case F_minus:
    return "-";
    break;
  case F_multiplication:
    return "*";
    break;
  case F_division:
    return "/";
    break;
  case F_modulo:
    return "%";
    break;
  case F_bitShiftLeft:
    return "<<";
    break;
  case F_bitShiftRight:
    return ">>";
    break;
  case F_equal:
    return "==";
    break;
  case F_notEqual:
    return "!=";
    break;
  case F_lessThen:
    return "<";
    break;
  case F_lessThenOrEqual:
    return "<=";
    break;
  case F_greaterThen:
    return ">";
    break;
  case F_greaterThenOrEqual:
    return ">=";
    break;
  case F_and:
    return "&&";
    break;
  case F_or:
    return "||";
    break;
  case F_plusplus:
    return "++";
    break;
  case F_minmin:
    return "--";
    break;
  }
  return "unknown key";
}

uint8_t stringToToken(const char * token, const char * value)
{
  if (strcmp(token, "INTEGER_CONST") == 0)
    return F_integerConstant;
  else if (strcmp(token, "REAL_CONST") == 0)
    return F_realConstant;
  else if (strcmp(value, "+") == 0)
    return F_plus;
  else if (strcmp(value, "-") == 0)
    return F_minus;
  else if (strcmp(value, "*") == 0)
    return F_multiplication;
  else if (strcmp(value, "/") == 0)
    return F_division;
  else if (strcmp(value, "%") == 0)
    return F_modulo;
  else if (strcmp(value, "<<") == 0)
    return F_bitShiftLeft;
  else if (strcmp(value, ">>") == 0)
    return F_bitShiftRight;
  else if (strcmp(value, "==") == 0)
    return F_equal;
  else if (strcmp(value, "!=") == 0)
    return F_notEqual;
  else if (strcmp(value, ">") == 0)
    return F_greaterThen;
  else if (strcmp(value, ">=") == 0)
    return F_greaterThenOrEqual;
  else if (strcmp(value, "<") == 0)
    return F_lessThen;
  else if (strcmp(value, "<=") == 0)
    return F_lessThenOrEqual;
  else if (strcmp(value, "&&") == 0)
    return F_and;
  else if (strcmp(value, "||") == 0)
    return F_or;
  else if (strcmp(value, "+=") == 0)
    return F_plus;
  else if (strcmp(value, "-=") == 0)
    return F_minus;
  else if (strcmp(value, "*=") == 0)
    return F_multiplication;
  else if (strcmp(value, "/=") == 0)
    return F_division;
  else if (strcmp(value, "++") == 0)
    return F_plusplus;
  else if (strcmp(value, "--") == 0)
    return F_minmin;
  else
    return F_NoToken;

}

enum Nodes
{
  F_Program,
  F_Function,
  F_Call,
  F_VarDef,
  F_Assign,
  F_Formal,
  F_VarRef,
  F_For,
  F_If,
  F_Cex,
  F_Expr,
  F_Term,
  #ifdef OPTIMIZED_TREE
    F_Statement,
    F_Indices,
    F_Formals,
    F_Factor,
    F_Block,
    F_Actuals,
    F_Increment,
    F_AssignOperator,
  #endif
  F_NoNode = 255
};

//Tokens
// ID

//Optimizer
// level
// index
// external

// block
// formals
// actuals
// increment
// assignoperator
// type (not used yet in wled)

//expr
//indices

const char * nodeToString(uint8_t key)
{
  switch (key) {
  case F_Program:
    return "program";
  case F_Function:
    return "function";
  case F_Call:
    return "call";
  case F_VarDef:
    return "variable";
  case F_Assign:
    return "assign";
  case F_Formal:
    return "formal";
  case F_VarRef:
    return "varref";
  case F_For:
    return "for";
  case F_If:
    return "if";
  case F_Cex:
    return "cex";
  case F_Expr:
    return "expr";
  case F_Term:
    return "term";
  #ifdef OPTIMIZED_TREEX
    case F_Statement:
      return "statement";
    case F_Indices:
      return "indices";
    case F_Formals:
      return "formals";
    case F_Factor:
      return "factor";
    case F_Block:
      return "block";
    case F_Actuals:
      return "actuals";
  #endif
  }
  return "unknown key";
}

uint8_t stringToNode(const char * node)
{
  if (strcmp(node, "program") == 0)
    return F_Program;
  else if (strcmp(node, "function") == 0)
    return F_Function;
  else if (strcmp(node, "call") == 0)
    return F_Call;
  else if (strcmp(node, "variable") == 0)
    return F_VarDef;
  else if (strcmp(node, "assign") == 0)
    return F_Assign;
  else if (strcmp(node, "formal") == 0)
    return F_Formal;
  else if (strcmp(node, "varref") == 0)
    return F_VarRef;
  else if (strcmp(node, "for") == 0)
    return F_For;
  else if (strcmp(node, "if") == 0)
    return F_If;
  else if (strcmp(node, "cex") == 0)
    return F_Cex;
  else if (strcmp(node, "expr") == 0)
    return F_Expr;
  else if (strcmp(node, "term") == 0)
    return F_Term;
  #ifdef OPTIMIZED_TREE
    else if (strcmp(node, "statement") == 0)
      return F_Statement;
    else if (strcmp(node, "indices") == 0)
      return F_Indices;
    else if (strcmp(node, "formals") == 0)
      return F_Formals;
    else if (strcmp(node, "factor") == 0)
      return F_Factor;
    else if (strcmp(node, "block") == 0)
      return F_Block;
    else if (strcmp(node, "actuals") == 0)
      return F_Actuals;
    else if (strcmp(node, "increment") == 0)
      return F_Increment;
    else if (strcmp(node, "assignoperator") == 0)
      return F_AssignOperator;
  #endif
  return F_NoNode;
}

bool errorOccurred = false;

struct Token {
    uint16_t lineno;
    uint16_t column;
    char type[charLength];
    char value[charLength]; 
};

struct LexerPosition {
  uint16_t pos;
  char current_char;
  uint16_t lineno;
  uint16_t column;
  char type[charLength];
  char value[charLength];
};

#define nrOfPositions 20

class Lexer {
  private:
  public:
    const char * text;
    uint16_t pos;
    char current_char;
    uint16_t lineno;
    uint16_t column;
    JsonObject definitionJson;
    Token current_token;
    LexerPosition positions[nrOfPositions]; //should be array of pointers but for some reason get seg fault (because a struct and not a class...)
    uint8_t positions_index = 0;

  Lexer(const char * programText, JsonObject definitionJson) {
    this->text = programText;
    this->definitionJson = definitionJson;
    this->pos = 0;
    this->current_char = this->text[this->pos];
    this->lineno = 1;
    this->column = 1;
  }

  ~Lexer() {
    DEBUG_ARTI("Destruct Lexer\n");
  }

  void advance() {
    if (this->current_char == '\n') 
    {
      this->lineno += 1;
      this->column = 0;
    }
    this->pos++;

    if (this->pos > strlen(this->text) - 1)
      this->current_char = -1;
    else 
    {
      this->current_char = this->text[this->pos];
      this->column++;
    }
  }

  void skip_whitespace() 
  {
    while (this->current_char != -1 && isspace(this->current_char))
      this->advance();
  }

  void skip_comment(const char * endTokens) 
  {
    while (strncmp(this->text + this->pos, endTokens, strlen(endTokens)) != 0)
      this->advance();
    for (size_t i=0; i<strlen(endTokens); i++)
      this->advance();
  }

  void number() 
  {
    current_token.lineno = this->lineno;
    current_token.column = this->column;
    strcpy(current_token.type, "");
    strcpy(current_token.value, "");

    char result[charLength] = "";
    while (this->current_char != -1 && isdigit(this->current_char)) 
    {
      result[strlen(result)] = this->current_char;
      this->advance();
    }
    if (this->current_char == '.') 
    {
      result[strlen(result)] = this->current_char;
      this->advance();

      while (this->current_char != -1 && isdigit(this->current_char)) 
      {
        result[strlen(result)] = this->current_char;
        this->advance();
      }

      result[strlen(result)] = '\0';
      strcpy(current_token.type, "REAL_CONST");
      strcpy(current_token.value, result);
    }
    else 
    {
      result[strlen(result)] = '\0';
      strcpy(current_token.type, "INTEGER_CONST");
      strcpy(current_token.value, result);
    }

  }

  void id() 
  {
    current_token.lineno = this->lineno;
    current_token.column = this->column;
    strcpy(current_token.type, "");
    strcpy(current_token.value, "");

    char result[charLength] = "";
    while (this->current_char != -1 && (isalnum(this->current_char) || this->current_char == '_')) 
    {
        result[strlen(result)] = this->current_char;
        this->advance();
    }
    result[strlen(result)] = '\0';

    char resultUpper[charLength];
    strcpy(resultUpper, result);
    strupr(resultUpper);

    if (definitionJson["TOKENS"].containsKey(resultUpper)) 
    {
      strcpy(current_token.type, definitionJson["TOKENS"][resultUpper]);
      strcpy(current_token.value, resultUpper);
    }
    else 
    {
      strcpy(current_token.type, "ID");
      strcpy(current_token.value, result);
    }
  }

  void get_next_token() 
  {
    current_token.lineno = this->lineno;
    current_token.column = this->column;
    strcpy(current_token.type, "");
    strcpy(current_token.value, "");

    if (errorOccurred) return;

    while (this->current_char != -1 && this->pos <= strlen(this->text) - 1 && !errorOccurred) 
    {
      if (isspace(this->current_char)) {
        this->skip_whitespace();
        continue;
      }

      if (strncmp(this->text + this->pos, "/*", 2) == 0) 
      {
        this->advance();
        skip_comment("*/");
        continue;
      }

      if (strncmp(this->text + this->pos, "//", 2) == 0) 
      {
        this->advance();
        skip_comment("\n");
        continue;
      }

      if (isalpha(this->current_char)) 
      {
        this->id();
        return;
      }
      
      if (isdigit(this->current_char) || (this->current_char == '.' && isdigit(this->text[this->pos+1])))
      {
        this->number();
        return;
      }

      // findLongestMatchingToken
      char token_type[charLength] = "";
      char token_value[charLength] = "";

      uint8_t longestTokenLength = 0;

      for (JsonPair tokenPair: definitionJson["TOKENS"].as<JsonObject>()) {
        const char * value = tokenPair.value();
        char currentValue[charLength+1];
        strncpy(currentValue, this->text + this->pos, charLength);
        currentValue[strlen(value)] = '\0';
        if (strcmp(value, currentValue) == 0 && strlen(value) > longestTokenLength) {
          strcpy(token_type, tokenPair.key().c_str());
          strcpy(token_value, value);
          longestTokenLength = strlen(value);
        }
      }

      if (strcmp(token_type, "") != 0 && strcmp(token_value, "") != 0) 
      {
        strcpy(current_token.type, token_type);
        strcpy(current_token.value, token_value);
        for (size_t i=0; i<strlen(token_value); i++)
          this->advance();
        return;
      }
      else {
        ERROR_ARTI("Lexer error on %c line %u col %u\n", this->current_char, this->lineno, this->column);
        errorOccurred = true;
      }
    }
  } //get_next_token

  void eat(const char * token_type) {
    // DEBUG_ARTI("try to eat %s %s\n", lexer->current_token.type, token_type);
    if (strcmp(current_token.type, token_type) == 0) {
      get_next_token();
      // DEBUG_ARTI("eating %s -> %s %s\n", token_type, lexer->current_token.type, lexer->current_token.value);
    }
    else {
      ERROR_ARTI("Lexer Error: Unexpected token %s %s\n", current_token.type, current_token.value);
      errorOccurred = true;
    }
  }

  void push_position() {
    if (positions_index < nrOfPositions) {
      positions[positions_index].pos = this->pos;
      positions[positions_index].current_char = this->current_char;
      positions[positions_index].lineno = this->lineno;
      positions[positions_index].column = this->column;
      strcpy(positions[positions_index].type, current_token.type);
      strcpy(positions[positions_index].value, current_token.value);
      positions_index++;
    }
    else
      ERROR_ARTI("not enough positions %u\n", nrOfPositions);
  }

  void pop_position() {
    if (positions_index > 0) {
      positions_index--;
      this->pos = positions[positions_index].pos;
      this->current_char = positions[positions_index].current_char;
      this->lineno = positions[positions_index].lineno;
      this->column = positions[positions_index].column;
      strcpy(current_token.type, positions[positions_index].type);
      strcpy(current_token.value, positions[positions_index].value);
    }
    else
      ERROR_ARTI("no positions saved\n");
  }

}; //Lexer

#define ResultFail 0
#define ResultStop 2
#define ResultContinue 1

class ScopedSymbolTable; //forward declaration

class Symbol {
  private:
  public:
  
  uint8_t symbol_type;
  char name[charLength];
  uint8_t type;
  uint8_t scope_level;
  uint8_t scope_index;
  ScopedSymbolTable* scope = nullptr;
  ScopedSymbolTable* function_scope = nullptr; //used to find the formal parameters in the scope of a function node

  JsonVariant block;

  Symbol(uint8_t symbol_type, const char * name, uint8_t type = 9) {
    this->symbol_type = symbol_type;
    strcpy(this->name, name);
    this->type = type;
    this->scope_level = 0;
  }

  ~Symbol() {
    MEMORY_ARTI("Destruct Symbol %s (%u)\n", name, FREE_SIZE);
  }

}; //Symbol

#define nrOfSymbolsPerScope 30
#define nrOfChildScope 10 //add checks

class ScopedSymbolTable {
  private:
  public:

  Symbol* symbols[nrOfSymbolsPerScope];
  uint8_t symbolsIndex = 0;
  uint8_t nrOfFormals = 0;
  char scope_name[charLength];
  uint8_t scope_level;
  ScopedSymbolTable *enclosing_scope;
  ScopedSymbolTable *child_scopes[nrOfChildScope];
  uint8_t child_scopesIndex = 0;

  ScopedSymbolTable(const char * scope_name, int scope_level, ScopedSymbolTable *enclosing_scope = nullptr) {
    strcpy(this->scope_name, scope_name);
    this->scope_level = scope_level;
    this->enclosing_scope = enclosing_scope;
  }

  ~ScopedSymbolTable() {
    for (uint8_t i=0; i<child_scopesIndex; i++) {
      delete child_scopes[i]; child_scopes[i] = nullptr;
    }
    for (uint8_t i=0; i<symbolsIndex; i++) {
      delete symbols[i]; symbols[i] = nullptr;
    }
    MEMORY_ARTI("Destruct ScopedSymbolTable %s (%u)\n", scope_name, FREE_SIZE);
  }

  void init_builtins() {
        // this->insert(BuiltinTypeSymbol('INTEGER'));
        // this->insert(BuiltinTypeSymbol('REAL'));
  }

  void insert(Symbol* symbol) 
  {
    #ifdef _SHOULD_LOG_SCOPE
      ANDBG_ARTI("Log scope Insert %s\n", symbol->name.c_str());
    #endif
    symbol->scope_level = this->scope_level;
    symbol->scope = this;
    symbol->scope_index = symbolsIndex;
    if (symbolsIndex < nrOfSymbolsPerScope)
      this->symbols[symbolsIndex++] = symbol;
    else
      ERROR_ARTI("ScopedSymbolTable %s symbols full (%d)", scope_name, nrOfSymbolsPerScope);
  }

  Symbol* lookup(const char * name, bool current_scope_only=false) 
  {
    for (uint8_t i=0; i<symbolsIndex; i++) {
      if (strcmp(symbols[i]->name, name) == 0)
        return symbols[i];
    }

    if (current_scope_only)
      return nullptr;
    // # recursively go up the chain and lookup the name;
    if (this->enclosing_scope != nullptr)
      return this->enclosing_scope->lookup(name);
    
    return nullptr;
  } //lookup

}; //ScopedSymbolTable

#define nrOfVariables 20

class ActivationRecord 
{
  private:
  public:
    char name[charLength];
    char type[charLength];
    int nesting_level;
    // char charMembers[charLength][nrOfVariables];
    float floatMembers[nrOfVariables];
    char lastSet[charLength];
    uint8_t lastSetIndex;

    ActivationRecord(const char * name, const char * type, int nesting_level) 
    {
        strcpy(this->name, name);
        strcpy(this->type, type);
        this->nesting_level = nesting_level;
    }

    ~ActivationRecord() 
    {
      RUNLOG_ARTI("Destruct activation record %s\n", name);
    }

    // void set(uint8_t index,  const char * value) 
    // {
    //   lastSetIndex = index;
    //   strcpy(charMembers[index], value);
    // }

    void set(uint8_t index, float value) 
    {
      lastSetIndex = index;
      floatMembers[index] = value;
    }

    // const char * getChar(uint8_t index) 
    // {
    //   return charMembers[index];
    // }

    float getFloat(uint8_t index) 
    {
      return floatMembers[index];
    }

}; //ActivationRecord

#define nrOfRecords 20

class CallStack {
public:
  ActivationRecord* records[nrOfRecords];
  uint8_t recordsCounter = 0;

  CallStack() {
  }

  ~CallStack() 
  {
    RUNLOG_ARTI("Destruct callstack\n");
  }

  void push(ActivationRecord* ar) 
  {
    if (recordsCounter < nrOfRecords) 
    {
      // RUNLOG_ARTI("%s\n", "Push ", ar->name);
      this->records[recordsCounter++] = ar;
    }
    else 
    {
      errorOccurred = true;
      ERROR_ARTI("no space left in callstack\n");
    }
  }

  ActivationRecord* pop()
  {
    if (recordsCounter > 0)
    {
      // RUNLOG_ARTI("%s\n", "Pop ", this->peek()->name);
      return this->records[recordsCounter--];
    }
    else 
    {
      ERROR_ARTI("no ar left on callstack\n");
      return nullptr;
    }
  }

  ActivationRecord* peek() 
  {
    return this->records[recordsCounter-1];
  }
}; //CallStack

class ValueStack 
{
private:
public:
  // char charStack[arrayLength][charLength]; //currently only floatStack used.
  float floatStack[arrayLength];
  uint8_t stack_index = 0;

  ValueStack() 
  {
  }

  ~ValueStack() 
  {
    RUNLOG_ARTI("Destruct valueStack\n");
  }

  // void push(const char * value) {
  //   if (stack_index >= arrayLength)
  //     ERROR_ARTI("Push charStack full %u of %u\n", stack_index, arrayLength);
  //   else if (value == nullptr) {
  //     strcpy(charStack[stack_index++], "empty");
  //     ERROR_ARTI("Push null pointer on float stack\n");
  //   }
  //   else
  //     // RUNLOG_ARTI("calc push %s %s\n", key, value);
  //     strcpy(charStack[stack_index++], value);
  // }

  void push(float value) 
  {
    if (stack_index >= arrayLength) 
    {
      ERROR_ARTI("Push floatStack full (check functions with result assigned) %u\n", arrayLength);
      errorOccurred = true;
    }
    else if (value == floatNull)
      ERROR_ARTI("Push null value on float stack\n");
    else
      // RUNLOG_ARTI("calc push %s %s\n", key, value);
      floatStack[stack_index++] = value;
  }

  // const char * peekChar() {
  //   // RUNLOG_ARTI("Calc Peek %s\n", charStack[stack_index-1]);
  //   return charStack[stack_index-1];
  // }

  float peekFloat() 
  {
    // RUNLOG_ARTI("Calc Peek %s\n", floatStack[stack_index-1]);
    return floatStack[stack_index-1];
  }

  // const char * popChar() {
  //   if (stack_index>0) {
  //     stack_index--;
  //     return charStack[stack_index];
  //   }
  //   else {
  //     ERROR_ARTI("Pop value stack empty\n");
        // errorOccurred = true;
//   // RUNLOG_ARTI("Calc Pop %s\n", charStack[stack_index]);
  //     return "novalue";
  //   }
  // }

  float popFloat() 
  {
    if (stack_index>0) 
    {
      stack_index--;
      return floatStack[stack_index];
    }
    else 
    {
      ERROR_ARTI("Pop floatStack empty\n");
    // RUNLOG_ARTI("Calc Pop %s\n", floatStack[stack_index]);
      errorOccurred = true;
      return -1;
    }
  }

}; //ValueStack

#define programTextSize 5000

class ARTI {
private:
  Lexer *lexer = nullptr;

  PSRAMDynamicJsonDocument *definitionJsonDoc = nullptr;
  PSRAMDynamicJsonDocument *parseTreeJsonDoc = nullptr;
  JsonObject definitionJson;
  JsonVariant parseTreeJson;

  ScopedSymbolTable *global_scope = nullptr;
  CallStack *callStack = nullptr;
  ValueStack *valueStack = nullptr;

  uint8_t stages = 5; //for debugging: 0:parseFile, 1:Lexer, 2:parse, 3:optimize, 4:analyze, 5:interpret should be 5 if no debugging

  char logFileName[fileNameLength];

  uint32_t startMillis;

public:
  ARTI() 
  {
    // MEMORY_ARTI("new Arti < %u\n", FREE_SIZE); //logfile not open here
  }

  ~ARTI() 
  {
    MEMORY_ARTI("Destruct ARTI\n");
  }

  //defined in arti_definition.h e.g. arti_wled.h!
  float arti_external_function(uint8_t function, float par1 = floatNull, float par2 = floatNull, float par3 = floatNull, float par4 = floatNull, float par5 = floatNull);
  float arti_get_external_variable(uint8_t variable, float par1 = floatNull, float par2 = floatNull, float par3 = floatNull);
  void arti_set_external_variable(float value, uint8_t variable, float par1 = floatNull, float par2 = floatNull, float par3 = floatNull);
  bool loop(); 

  uint8_t parse(JsonVariant parseTree, const char * node_name, char operatorx, JsonVariant expression, uint8_t depth = 0) 
  {
    if (depth > 50) 
    {
      ERROR_ARTI("Error: Parse recursion level too deep at %s (%u)\n", asChar(parseTree), depth);
      errorOccurred = true;
    }
    if (errorOccurred) return ResultFail;

    uint8_t result = ResultContinue;

    uint8_t resultChild = ResultContinue;

    if (expression.is<JsonArray>()) //should always be the case
    {
      for (JsonVariant expressionElement: expression.as<JsonArray>()) //e.g. ["PROGRAM","ID","block"]
      {
        const char * nextNode_name = node_name; //e.g. "program": 
        JsonVariant nextExpression = expressionElement; // e.g. block
        JsonVariant nextParseTree = parseTree;

        JsonVariant nodeExpression = lexer->definitionJson[expressionElement.as<const char *>()];

        if (!nodeExpression.isNull()) //is expressionElement a Node e.g. "block" : ["LCURL",{"*": ["statement"]},"RCURL"]
        {
          nextNode_name = expressionElement; //e.g. block
          nextExpression = nodeExpression; // e.g. ["LCURL",{"*": ["statement"]},"RCURL"]

          // DEBUG_ARTI("%s %s %u\n", spaces+50-depth, nextNode_name, depth); //, asChar(parseTree)

          if (parseTree.is<JsonArray>()) 
          {
            parseTree[parseTree.size()][nextNode_name]["connect"] = "array";
            nextParseTree = parseTree[parseTree.size()-1]; //nextparsetree is last element in the array (which is always an object)
          }
          else //no list, create object
          { 
            if (parseTree[node_name].isNull()) //no object yet
              parseTree[node_name]["connect"] = "object"; //make the connection, new object item

            nextParseTree = parseTree[node_name];
          }
        }

        if (operatorx == '|')
          lexer->push_position();

        if (nextExpression.is<JsonObject>()) // e.g. {"?":["LPAREN","formals*","RPAREN"]}
        {
          JsonObject::iterator objectIterator = nextExpression.as<JsonObject>().begin();
          char objectOperator = objectIterator->key().c_str()[0];
          JsonVariant objectElement = objectIterator->value();

          if (objectElement.is<JsonArray>()) 
          {
            if (objectOperator == '*' || objectOperator == '+') 
            {
              nextParseTree[nextNode_name]["*"][0] = "multiple"; // * is another object in the list of objects
              nextParseTree = nextParseTree[nextNode_name]["*"];
            }

            //and: see 'is array'
            if (objectOperator == '|') 
            {
              resultChild = parse(nextParseTree, nextNode_name, objectOperator, objectElement, depth + 1);
              if (resultChild != ResultFail) resultChild = ResultContinue;
            }
            else 
            {
              uint8_t resultChild2 = ResultContinue;
              uint8_t counter = 0;
              while (resultChild2 == ResultContinue) 
              {
                resultChild2 = parse(nextParseTree, nextNode_name, objectOperator, objectElement, depth + 1); //no assign to result as optional

                if (objectOperator == '?') { //zero or one iteration, also continue if parse not continue
                  resultChild2 = ResultContinue;
                  break; 
                }
                else if (objectOperator == '+') { //one or more iterations, stop if first parse not continue
                  if (counter == 0) {
                    if (resultChild2 != ResultContinue)
                      break;
                  } 
                  else 
                  {
                    if (resultChild2 != ResultContinue) 
                    {
                      resultChild2 = ResultContinue;  //always continue
                      break;
                    }
                  }
                }
                else if (objectOperator == '*') //zero or more iterations, stop if parse not continue
                {
                  if (resultChild2 != ResultContinue) 
                  {
                    resultChild2 = ResultContinue;  //always continue
                    break;
                  }
                }
                else 
                {
                  ERROR_ARTI("%s Programming error: undefined %c %s\n", spaces+50-depth, objectOperator, asChar(objectElement));
                  resultChild2 = ResultFail;
                }
                counter++;
              } //while
              resultChild = resultChild2;
            } //not or
          } // element is array
          else
            ERROR_ARTI("%s Definition error: should be an array %s %c %s\n", spaces+50-depth, stringOrEmpty(nextNode_name), operatorx, asChar(objectElement));
        }
        else if (nextExpression.is<JsonArray>()) // e.g. ["LPAREN", "expr*", "RPAREN"]
        {
          resultChild = parse(nextParseTree, nextNode_name, '&', nextExpression, depth + 1); // every array element starts with '&' (operatorx is for result of all elements of array)
        }
        else if (lexer->definitionJson["TOKENS"].containsKey(nextExpression.as<const char *>())) // token e.g. "ID"
        {
          const char * token_type = nextExpression;
          if (strcmp(lexer->current_token.type, token_type) == 0) 
          {
            DEBUG_ARTI("%s %s %s", spaces+50-depth, lexer->current_token.type, lexer->current_token.value);

            //only add 'semantic tokens'
            if (strcmp(lexer->current_token.type, "ID") != 0 && strcmp(lexer->current_token.type, "INTEGER_CONST") != 0 && strcmp(lexer->current_token.type, "REAL_CONST") != 0 && 
                          strcmp(lexer->current_token.type, "INTEGER") != 0 && strcmp(lexer->current_token.type, "REAL") != 0 && 
                          strcmp(lexer->current_token.value, "+") != 0 && strcmp(lexer->current_token.value, "-") != 0 && strcmp(lexer->current_token.value, "*") != 0 && strcmp(lexer->current_token.value, "/") != 0 && strcmp(lexer->current_token.value, "%") != 0 && 
                          strcmp(lexer->current_token.value, "+=") != 0 && strcmp(lexer->current_token.value, "-=") != 0 && strcmp(lexer->current_token.value, "*=") != 0 && strcmp(lexer->current_token.value, "/=") != 0 &&
                          strcmp(lexer->current_token.value, "<<") != 0 && strcmp(lexer->current_token.value, ">>") != 0 && 
                          strcmp(lexer->current_token.value, "==") != 0 && strcmp(lexer->current_token.value, "!=") != 0 && 
                          strcmp(lexer->current_token.value, "&&") != 0 && strcmp(lexer->current_token.value, "||") != 0 && 
                          strcmp(lexer->current_token.value, ">") != 0 && strcmp(lexer->current_token.value, ">=") != 0 && strcmp(lexer->current_token.value, "<") != 0 && strcmp(lexer->current_token.value, "<=") != 0 &&
                          strcmp(lexer->current_token.value, "++") != 0 && strcmp(lexer->current_token.value, "--") != 0
            )
            {}
            else
            {
              if (nextParseTree.is<JsonArray>()) 
                nextParseTree.as<JsonArray>()[nextParseTree.size()][lexer->current_token.type] = lexer->current_token.value; //add in last element of array
              else
                nextParseTree[nextNode_name][lexer->current_token.type] = lexer->current_token.value;
            }

            lexer->eat(token_type);

            DEBUG_ARTI(" -> [%s %s] %d\n", lexer->current_token.type, lexer->current_token.value, depth);

            resultChild = ResultContinue;
          }
          else //deadend
            resultChild = ResultFail;
        } // if token
        else //expressionElement is not a node, not a token, not an array and not an object
        {
          if (lexer->definitionJson.containsKey(nextExpression.as<const char *>()))
            ERROR_ARTI("%s Programming error: %s not a node, token, array or object in %s\n", spaces+50-depth, asChar(nextExpression), stringOrEmpty(nextNode_name));
          else
            ERROR_ARTI("%s Definition error: \"%s\": \"%s\" node should be embedded in array\n", spaces+50-depth, stringOrEmpty(nextNode_name), asChar(nextExpression));
        } //nextExpression is not a token

        if (!nodeExpression.isNull()) //if node
        {
          if (nextParseTree.containsKey("connect"))
            nextParseTree.remove("connect"); //remove connector
          // for values which are not parsed deeper. e.g. : {"formal": {"ID": "z"}}
          for (JsonPair parseTreePair : nextParseTree.as<JsonObject>()) 
          {
            JsonVariant value = parseTreePair.value();
            if (value.containsKey("connect"))
              value.remove("connect"); //remove connector
          }

          if (resultChild == ResultFail) { //remove result of parse
            nextParseTree.remove(nextNode_name); //remove the failed stuff

            // DEBUG_ARTI("%s fail %s\n", spaces+50-depth, nextNode_name);
          }
          else //success
          {
            DEBUG_ARTI("%s found %s\n", spaces+50-depth, nextNode_name);//, asChar(nextParseTree));
            //parseTree optimization moved to optimize function

            // if (nextParseTree.is<JsonObject>())
            // {

            //   // optimize(nextParseTree, depth);

            //   JsonObject innerObject = nextParseTree[nextNode_name].as<JsonObject>();

            //   JsonObject::iterator begin = innerObject.begin();
            //   if (fnextParseTree.size() == 1 && nextParseTree[nextNode_name].size() == 1 && lexer->definitionJson.containsKey(nextNode_name) && lexer->definitionJson.containsKey(nextNode_name) && lexer->definitionJson.containsKey(begin->key()))
            //   {
            //     // JsonObject nextParseTreeObject = nextParseTree.as<JsonObject>();

            //     DEBUG_ARTI("%s found %s:%s\n", spaces+50-depth, node_name, asChar(parseTree));
            //     DEBUG_ARTI("%s found replace %s by %s %s\n", spaces+50-depth, nextNode_name, begin->key().c_str(), asChar(nextParseTree));
            //     DEBUG_ARTI("%s expression %s\n", spaces+50-depth, asChar(expression));
            //     DEBUG_ARTI("%s found %s\n", spaces+50-depth, asChar(nextParseTree[nextNode_name]));

            //     nextParseTree.remove(nextNode_name);
            //     // char temp[charLength];
            //     // strcpy(temp, nextNode_name);
            //     // strcat(temp, "-");
            //     // strcat(temp, begin->key().c_str());
            //     nextParseTree[begin->key()] = begin->value();

            //     DEBUG_ARTI("%s found %s:%s\n", spaces+50-depth, node_name, asChar(parseTree));
            //   }
            // }
            // else
            //   DEBUG_ARTI("%s no jsonobject??? %s\n", spaces+50-depth, asChar(parseTree));

          }
        } // if node

        //determine result of expressionElement
        if (operatorx == '|') {
          if (resultChild == ResultFail) {//if fail, go back and try another
            // result = ResultContinue;
            lexer->pop_position();
          }
          else {
            result = ResultStop;  //Stop or continue is enough for an or
            lexer->positions_index--;
          }
        }
        else {
          if (resultChild != ResultContinue) //for and, ?, + and *; each result should continue
            result = ResultFail;
        } 

        if (result != ResultContinue) //if no reason to continue then stop
          break;

      } //for expressionElement

      if (operatorx == '|') 
      {
        if (result != ResultStop) //still looking but nothing to look for
          result = ResultFail;
      }
    }
    else { //should never happen
      ERROR_ARTI("%s Programming error: no array %s %c %s\n", spaces+50-depth, stringOrEmpty(node_name), operatorx, asChar(expression));
    }

    return result;

  } //parse

  bool analyze(JsonVariant parseTree, const char * treeElement = nullptr, ScopedSymbolTable* current_scope = nullptr, uint8_t depth = 0) 
  {
    // ANDBG_ARTI("%s Depth %u %s\n", spaces+50-depth, depth, asChar(parseTree));
    if (depth > 24) //otherwise stack canary errors on Arduino (value determined after testing, should be revisited)
    {
      ERROR_ARTI("Error: Analyze recursion level too deep at %s (%u)\n", asChar(parseTree), depth);
      errorOccurred = true;
    }
    if (errorOccurred) return false;

    if (parseTree.is<JsonObject>()) 
    {
      for (JsonPair parseTreePair : parseTree.as<JsonObject>()) 
      {
        const char * key = parseTreePair.key().c_str();
        JsonVariant value = parseTreePair.value();
        if (treeElement == nullptr || strcmp(treeElement, key) == 0 ) //in case there are more elements in the object and you want to analyze only one
        {
          bool visitedAlready = false;

          if (strcmp(key, "*") == 0 ) // multiple
          {
          }
          else if (strcmp(key, "token") == 0) // do nothing with added tokens
            visitedAlready = true;
          else if (this->definitionJson["TOKENS"].containsKey(key)) // if token
          {
           const char * valueStr = value;

            if (strcmp(key, "INTEGER_CONST") == 0)
              parseTree["token"] = F_integerConstant;
            else if (strcmp(key, "REAL_CONST") == 0)
              parseTree["token"] = F_realConstant;
            else if (strcmp(valueStr, "+") == 0)
              parseTree["token"] = F_plus;
            else if (strcmp(valueStr, "-") == 0)
              parseTree["token"] = F_minus;
            else if (strcmp(valueStr, "*") == 0)
              parseTree["token"] = F_multiplication;
            else if (strcmp(valueStr, "/") == 0)
              parseTree["token"] = F_division;
            else if (strcmp(valueStr, "%") == 0)
              parseTree["token"] = F_modulo;
            else if (strcmp(valueStr, "<<") == 0)
              parseTree["token"] = F_bitShiftLeft;
            else if (strcmp(valueStr, ">>") == 0)
              parseTree["token"] = F_bitShiftRight;
            else if (strcmp(valueStr, "==") == 0)
              parseTree["token"] = F_equal;
            else if (strcmp(valueStr, "!=") == 0)
              parseTree["token"] = F_notEqual;
            else if (strcmp(valueStr, ">") == 0)
              parseTree["token"] = F_greaterThen;
            else if (strcmp(valueStr, ">=") == 0)
              parseTree["token"] = F_greaterThenOrEqual;
            else if (strcmp(valueStr, "<") == 0)
              parseTree["token"] = F_lessThen;
            else if (strcmp(valueStr, "<=") == 0)
              parseTree["token"] = F_lessThenOrEqual;
            else if (strcmp(valueStr, "&&") == 0)
              parseTree["token"] = F_and;
            else if (strcmp(valueStr, "||") == 0)
              parseTree["token"] = F_or;
            else
              ERROR_ARTI("%s Programming error: token not defined as operator %s %s (%u)\n", spaces+50-depth, key, asChar(value), depth);

            visitedAlready = true;
          }
          else //if key is node_name
          {
            uint8_t node = stringToNode(key);

            switch (node) 
            {
              case F_Program: 
              {
                const char * program_name = value["ID"];
                global_scope = new ScopedSymbolTable(program_name, 1, nullptr); //current_scope

                ANDBG_ARTI("%s Program %s %u %u\n", spaces+50-depth, global_scope->scope_name, global_scope->scope_level, global_scope->symbolsIndex); 

                if (value["ID"].isNull()) {
                  ERROR_ARTI("program name null\n");
                  errorOccurred = true;
                }
                if (value["block"].isNull()) {
                  ERROR_ARTI("%s Program %s: no block in parseTree\n", spaces+50-depth, program_name); 
                  errorOccurred = true;
                }
                else {
                  analyze(value["block"], nullptr, global_scope, depth + 1);
                }

                #ifdef ARTI_DEBUG
                  for (uint8_t i=0; i<global_scope->symbolsIndex; i++) {
                    Symbol* symbol = global_scope->symbols[i];
                    ANDBG_ARTI("%s %u %s %s.%s of %u (%u)\n", spaces+50-depth, i, nodeToString(symbol->symbol_type), global_scope->scope_name, symbol->name, symbol->type, symbol->scope_level); 
                  }
                #endif

                visitedAlready = true;
                break;
              }
              case F_Function: 
              {
                //find the function name (so we must know this is a function...)
                const char * function_name = value["ID"];
                Symbol* function_symbol = new Symbol(node, function_name);
                current_scope->insert(function_symbol);

                ANDBG_ARTI("%s Function %s.%s\n", spaces+50-depth, current_scope->scope_name, function_name);
                ScopedSymbolTable* function_scope = new ScopedSymbolTable(function_name, current_scope->scope_level + 1, current_scope);
                if (current_scope->child_scopesIndex < nrOfChildScope)
                  current_scope->child_scopes[current_scope->child_scopesIndex++] = function_scope;
                else
                  ERROR_ARTI("ScopedSymbolTable %s childs full (%d)", current_scope->scope_name, nrOfChildScope);
                function_symbol->function_scope = function_scope;

                if (value.containsKey("formals"))
                  analyze(value["formals"], nullptr, function_scope, depth + 1);

                function_scope->nrOfFormals = function_scope->symbolsIndex;

                if (value["block"].isNull())
                  ERROR_ARTI("%s Function %s: no block in parseTree\n", spaces+50-depth, function_name); 
                else
                  analyze(value["block"], nullptr, function_scope, depth + 1);

                #ifdef ARTI_DEBUG
                  for (uint8_t i=0; i<function_scope->symbolsIndex; i++) {
                    Symbol* symbol = function_scope->symbols[i];
                    ANDBG_ARTI("%s %u %s %s.%s of %u (%u)\n", spaces+50-depth, i, nodeToString(symbol->symbol_type), function_scope->scope_name, symbol->name, symbol->type, symbol->scope_level); 
                  }
                #endif

                visitedAlready = true;
                break;
              }
              case F_VarDef:
              case F_Formal:
              case F_Assign:
              case F_VarRef: 
              {
                JsonObject variable_value;
                if (node == F_Assign) 
                  variable_value = value["varref"];
                else
                  variable_value = value;

                const char * variable_name;
                variable_name = variable_value["ID"];

                if (variable_value.containsKey("indices"))
                  analyze(variable_value, "indices", current_scope, depth + 1);

                //check if external variable
                bool externalFound = false;
                uint8_t index = 0;
                for (JsonPair externalsPair: definitionJson["EXTERNALS"].as<JsonObject>()) {
                  if (strcmp(variable_name, externalsPair.key().c_str()) == 0) {
                      variable_value["external"] = index; //add external index to parseTree
                    ANDBG_ARTI("%s Ext Variable found %s (%u) %s\n", spaces+50-depth, variable_name, depth, key);
                    externalFound = true;
                  }
                  index++;
                }

                if (!externalFound) 
                {
                  Symbol* var_symbol = current_scope->lookup(variable_name); //lookup here and parent scopes
                  if (node == F_VarRef) 
                  {
                    if (var_symbol == nullptr) {
                      WARNING_ARTI("%s VarRef %s ID not found in scope of %s\n", spaces+50-depth, variable_name, current_scope->scope_name); 
                      //only warning: value 0 in interpreter (div 0 is captured)
                    } else { 
                      ANDBG_ARTI("%s VarRef found %s.%s (%u)\n", spaces+50-depth, var_symbol->scope->scope_name, variable_name, depth);
                    }
                  }
                  else //assign and var/formal
                  {
                    //if variable not already defined, then add
                    if (node != F_Assign || var_symbol == nullptr) //only assign needs to check if not exists
                    {
                      char param_type[charLength]; 
                      if (variable_value.containsKey("type")) 
                        serializeJson(variable_value["type"], param_type); //current_scope.lookup(param.type_node.value); //need string, lookup also used to find types...
                      else
                        strcpy(param_type, "notype");

                      var_symbol = new Symbol(node, variable_name, 9); // no type support yet
                      if (node == F_Assign)
                        global_scope->insert(var_symbol); // assigned variables are global scope
                      else
                        current_scope->insert(var_symbol);

                      ANDBG_ARTI("%s %s %s.%s of %s\n", spaces+50-depth, key, var_symbol->scope->scope_name, variable_name, param_type);
                    }
                    else if (node != F_Assign && var_symbol != nullptr)
                      ERROR_ARTI("%s %s Duplicate ID %s.%s\n", spaces+50-depth, key, var_symbol->scope->scope_name, variable_name); 

                  }

                  if (var_symbol != nullptr)
                  {
                    variable_value["level"] = var_symbol->scope_level;
                    variable_value["index"] = var_symbol->scope_index;;
                  }
                }

                if (node == F_Assign) 
                {
                  ANDBG_ARTI("%s %s %s = (%u)\n", spaces+50-depth, key, variable_name, depth);

                  if (value.containsKey("assignoperator")) 
                  {
                    JsonObject asop = value["assignoperator"];
                    JsonObject::iterator asopBegin = asop.begin();
                    ANDBG_ARTI("%s %s\n", spaces+50-depth, asChar(asopBegin->value()));
                    if (strcmp(asopBegin->value(), "+=") == 0) 
                      value["assignoperator"] = F_plus;
                    else if (strcmp(asopBegin->value(), "-=") == 0) 
                      value["assignoperator"] = F_minus;
                    else if (strcmp(asopBegin->value(), "*=") == 0) 
                      value["assignoperator"] = F_multiplication;
                    else if (strcmp(asopBegin->value(), "/=") == 0) 
                      value["assignoperator"] = F_division;
                    else if (strcmp(asopBegin->value(), "++") == 0) 
                      value["assignoperator"] = F_plusplus;
                    else if (strcmp(asopBegin->value(), "--") == 0) 
                      value["assignoperator"] = F_minmin;
                  }

                  if (value.containsKey("expr")) 
                    analyze(value, "expr", current_scope, depth + 1);
                  else if (value["assignoperator"].as<uint8_t>() != F_plusplus && value["assignoperator"].as<uint8_t>() != F_minmin)
                  {
                    ERROR_ARTI("%s %s %s: Assign without expression\n", spaces+50-depth, key, variable_name); 
                    errorOccurred = true;
                  }
                }

                visitedAlready = true;
                break;
              }
              case F_Call: 
              {
                const char * function_name = value["ID"];

                //check if external function
                bool externalFound = false;
                uint8_t index = 0;
                for (JsonPair externalsPair: definitionJson["EXTERNALS"].as<JsonObject>()) 
                {
                  if (strcmp(function_name, externalsPair.key().c_str()) == 0) 
                  {
                    ANDBG_ARTI("%s Ext Function found %s (%u)\n", spaces+50-depth, function_name, depth);
                    value["external"] = index; //add external index to parseTree 
                    externalFound = true;
                  }
                  index++;
                }

                if (!externalFound) 
                {
                  Symbol* function_symbol = current_scope->lookup(function_name); //lookup here and parent scopes
                  if (function_symbol != nullptr) 
                    analyze(function_symbol->block, nullptr, current_scope, depth + 1);
                  else 
                    ERROR_ARTI("%s Function %s not found in scope of %s\n", spaces+50-depth, function_name, current_scope->scope_name); 
                } //external functions

                if (value.containsKey("actuals"))
                  analyze(value["actuals"], nullptr, current_scope, depth + 1);

                visitedAlready = true;
                break;
              } // case
              default: //visitedalready false => recursive call
                break;
            } //switch
          } // is node_name

          if (!visitedAlready && value.size() > 0) // if size == 0 then injected key/value like operator
            analyze(value, nullptr, current_scope, depth + 1);

        } // key values
      } ///for elements in object
    }
    else if (parseTree.is<JsonArray>()) 
    {
      for (JsonVariant newParseTree: parseTree.as<JsonArray>()) 
        analyze(newParseTree, nullptr, current_scope, depth + 1);
    }
    else //not array
    {
      // string element = parseTree;
      //for some weird reason this causes a crash on esp32
      // ERROR_ARTI("%s Error: parseTree should be array or object %s (%u)\n", spaces+50-depth, asChar(parseTree), depth);
    }

    return !errorOccurred;
  } //analyze

  //https://dev.to/lefebvre/compilers-106---optimizer--ig8
  bool optimize(JsonVariant parseTree, uint8_t depth = 0) 
  {
    // DEBUG_ARTI("%s optimize %s (%u)\n", spaces+50-depth, asChar(parseTree), depth);

    if (parseTree.is<JsonObject>()) 
    {
      //make the parsetree as small as possible to let the interpreter run as fast as possible:

      for (JsonPair parseTreePair : parseTree.as<JsonObject>()) 
      {
        const char * key = parseTreePair.key().c_str();
        JsonVariant value = parseTreePair.value();

        bool visitedAlready = false;

        // object:
        // if key is node and value is object then optimize object after that if object empty then remove key
        // else key is !ID and value is string then remove key
        // else key is * and value is array then optimize array after that if array empty then remove key *

        // array
        // if element is multiple then remove

        if (strcmp(key, "*") == 0 ) // multiple
        {
          optimize(value, depth + 1);

          if (value.size() == 0) 
          {
            parseTree.remove(key);
            // DEBUG_ARTI("%s optimize: remove empty %s %s (%u)\n", spaces+50-depth, key, asChar(value), depth);
          }

          visitedAlready = true;
        }
        else if (this->definitionJson["TOKENS"].containsKey(key)) // if key is token (moved to parse)
        {
          visitedAlready = true;
        }
        else if (value.is<JsonObject>()) //if key is node_name
        {
          optimize(value, depth + 1);

          if (value.size() == 0) 
          {
            // DEBUG_ARTI("%s optimize: remove key %s with empty object (%u)\n", spaces+50-depth, key, depth);
            parseTree.remove(key);
          }
          else if (value.size() == 1) //try to shrink, moved to below
          {
            //- check if a node is not used in analyzer / interpreter and has only one element: go to the parent and replace itself with its child (shrink)

            // DEBUG_ARTI("%s node try to shrink %s : %s (%u)\n", spaces+50-depth, key, asChar(value), value.size());

            JsonObject::iterator objectIterator = value.as<JsonObject>().begin();

            if (definitionJson.containsKey(objectIterator->key().c_str()))  // if value key is a node
            {
              // if (objectIterator->value().is<JsonObject>() && objectIterator->value().size() == 1) //
              // {
              //   // JsonObject::iterator objectIterator2 = objectIterator->value().as<JsonObject>().begin();

              //   // if (objectIterator2->value().is<JsonObject>() ) //&& objectIterator.size() == 1
              //   {
              //     DEBUG_ARTI("%s node to shrink %s : %s from %s\n", spaces+50-depth, key, asChar(value), asChar(parseTree));
              //     DEBUG_ARTI("%s node to shrink %s : %s\n", spaces+50-depth, objectIterator->key().c_str(), asChar(objectIterator->value()));
              //     // DEBUG_ARTI("%s node to shrink %s : %s\n", spaces+50-depth, objectIterator2->key().c_str(), asChar(objectIterator2->value()));
              //     DEBUG_ARTI("%s node to shrink replace %s\n", spaces+50-depth, asChar(parseTree[key]));
              //     DEBUG_ARTI("%s node to shrink      by %s\n", spaces+50-depth, asChar(objectIterator->value()));
              //     // parseTree[key][objectIterator->key().c_str()] = objectIterator2->value();
              //     parseTree[key] = objectIterator->value();
              //   }
              //   // else
              //   // {
              //   //   DEBUG_ARTI("%s node to shrink2 %s : %s\n", spaces+50-depth, objectIterator2->key().c_str(), asChar(objectIterator2->value()));
              //   // }
              // }
              // else
              //   DEBUG_ARTI("%s value should be an object %s in %s : %s from %s\n", spaces+50-depth, objectIterator->key().c_str(), key, asChar(objectIterator->value()), asChar(value));
              if (stringToNode(objectIterator->key().c_str()) == F_NoNode) // if key not a node
              // if (objectIterator->value().size() == 1)
              {
                DEBUG_ARTI("%s node to shrink %s in %s : %s from %s\n", spaces+50-depth, objectIterator->key().c_str(), key, asChar(value), asChar(parseTree));
                // DEBUG_ARTI("%s node to shrink %s in %s = %s from %s\n", spaces+50-depth, objectIterator->key().c_str(), key, asChar(objectIterator->value()), asChar(parseTree[key]));
                 parseTree[key] = objectIterator->value();

                // parseTree[key]["old"] = objectIterator->key();

                // parseTreePair.key() = objectIterator->key();
                // parseTreePair._key = objectIterator->key();
                // parseTree[objectIterator->key()] = objectIterator->value();
                // parseTree.remove(key);
                // parseTree[key][objectIterator2->key().c_str()] = objectIterator2->value();
              }
            }
          } //shrink

          visitedAlready = true;
        }
        else
          ERROR_ARTI("%s Programming Error: key no node and no token %s %s (%u)\n", spaces+50-depth, key, asChar(value), depth);

        if (!visitedAlready && value.size() > 0) // if size == 0 then injected key/value like operator
          optimize(value, depth + 1);

      } ///for elements in object

      //shrink
      for (JsonPair parseTreePair : parseTree.as<JsonObject>()) 
      {
        const char * key = parseTreePair.key().c_str();
        JsonVariant value = parseTreePair.value();

        if (false && value.is<JsonObject>() && parseTree.size() == 1 && value.size() == 1 && definitionJson.containsKey(key)) //if key is node_name
        {
          JsonObject::iterator objectIterator = value.as<JsonObject>().begin();

          // DEBUG_ARTI("%s try replace %s by %s %s\n", spaces+50-depth, key, objectIterator->key().c_str(), asChar(parseTree));

          if (strcmp(objectIterator->key().c_str(), "ID") != 0) //&& definitionJson.containsKey(objectIterator->key())???
          {
            // DEBUG_ARTI("%s found %s:%s\n", spaces+50-depth, node_name, asChar(parseTree));
            // DEBUG_ARTI("%s found replace %s by %s %s\n", spaces+50-depth, key, objectIterator->key().c_str(), asChar(parseTree));
            // DEBUG_ARTI("%s found %s\n", spaces+50-depth, asChar(value));

            // DEBUG_ARTI("%s found %s\n", spaces+50-depth, asChar(parseTree));
            DEBUG_ARTI("%s replace %s by %s %s\n", spaces+50-depth, key, objectIterator->key().c_str(), asChar(parseTree));

            parseTree.remove(key);
            // parseTree[key] = value;
            parseTree[objectIterator->key()] = objectIterator->value();

            // DEBUG_ARTI("%s found %s:%s\n", spaces+50-depth, node_name, asChar(parseTree));

          }
          // else
          // {
          //   DEBUG_ARTI("%s not shrinkable %s %s\n", spaces+50-depth, key, asChar(value));
          //   if (depth > 12) {
          //   // parseTree.remove(key);
          //       char temp[charLength];
          //       strcpy(temp, key);
          //       strcat(temp, "-");
          //       // strcat(temp, objectIterator->key().c_str());
          //   // parseTree[temp] = value;
          //   }
          // }

          if (false && definitionJson.containsKey(objectIterator->key().c_str()))  // if value key is a node
          {
            if (stringToNode(objectIterator->key().c_str()) == F_NoNode) // if key not a node
            // if (objectIterator->value().size() == 1)
            {
              DEBUG_ARTI("%s node to shrink %s in %s : %s from %s\n", spaces+50-depth, objectIterator->key().c_str(), key, asChar(value), asChar(parseTree));
              // DEBUG_ARTI("%s node to shrink %s in %s = %s from %s\n", spaces+50-depth, objectIterator->key().c_str(), key, asChar(objectIterator->value()), asChar(parseTree[key]));
                parseTree[key] = objectIterator->value();

              // parseTree[key]["old"] = objectIterator->key();

              // parseTreePair.key() = objectIterator->key();
              // parseTreePair._key = objectIterator->key();
              // parseTree[objectIterator->key()] = objectIterator->value();
              // parseTree.remove(key);
              // parseTree[key][objectIterator2->key().c_str()] = objectIterator2->value();
            }
          }
        } //value is jsonObject

      } // for
    }
    else if (parseTree.is<JsonArray>())
    {
      JsonArray parseTreeArray = parseTree.as<JsonArray>();
      
      uint8_t arrayIndex = 0;
      for (JsonArray::iterator it = parseTreeArray.begin(); it != parseTreeArray.end(); ++it) 
      {
        JsonVariant element = *it;

        if (element == "multiple") 
        {
          // DEBUG_ARTI("%s optimize: remove array element 'multiple' of array (%u)\n", spaces+50-depth, arrayIndex);
          parseTreeArray.remove(it);
        }
        else if (it->size() == 0) //remove {} elements (added by * arrays, don't know where added)
        {
          // DEBUG_ARTI("%s optimize: remove array element {} of %s array (%u)\n", spaces+50-depth, element.as<std::string>().c_str(), arrayIndex);
          parseTreeArray.remove(it);
        }
        else 
          optimize(*it, depth + 1);

        arrayIndex++;
      }
    }
    else //not array
    {
      // string element = parseTree;
      //for some weird reason this causes a crash on esp32
      ERROR_ARTI("%s Error: parseTree should be array or object %s (%u)\n", spaces+50-depth, asChar(parseTree), depth);
    }

    // DEBUG_ARTI("%s optimized %s (%u)\n", spaces+50-depth, asChar(parseTree), depth);

    return !errorOccurred;
  } //optimize

  // bool visit_ID(JsonVariant parseTree, const char * treeElement = nullptr, ScopedSymbolTable* current_scope = nullptr, uint8_t depth = 0) 

  bool interpret(JsonVariant parseTree, const char * treeElement = nullptr, ScopedSymbolTable* current_scope = nullptr, uint8_t depth = 0) 
  {
    // RUNLOG_ARTI("%s Interpret %s %s (%u)\n", spaces+50-depth, stringOrEmpty(treeElement), asChar(parseTree), depth);

    if (depth >= 50) 
    {
      ERROR_ARTI("Error: Interpret recursion level too deep at %s (%u)\n", asChar(parseTree), depth);
      errorOccurred = true;
    }
    if (errorOccurred) return false;

    if (parseTree.is<JsonObject>()) 
    {
      for (JsonPair parseTreePair : parseTree.as<JsonObject>()) 
      {
        const char * key = parseTreePair.key().c_str();
        JsonVariant value = parseTreePair.value();
        if (treeElement == nullptr || strcmp(treeElement, key) == 0) 
        {
          // RUNLOG_ARTI("%s Interpret object element %s %s\n", spaces+50-depth, key, asChar(value));

          bool visitedAlready = false;

          if (strcmp(key, "*") == 0)
          {
            // do the recursive call below
          }
          else if (strcmp(key, "token") == 0 || strcmp(key, "variable") == 0) //variable decls done in analyze (see pas)
            visitedAlready = true;
          else if (parseTree.containsKey("token")) //key is token
          {
            // RUNLOG_ARTI("%s Token %s %s %s\n", spaces+50-depth, key, valueStr, asChar(parseTree));

            const char * valueStr = value;

            switch (parseTree["token"].as<uint8_t>()) 
            {
              case F_integerConstant:
              case F_realConstant:
                valueStack->push(atof(valueStr)); //push value
                #if ARTI_PLATFORM != ARTI_ARDUINO  //for some weird reason this causes a crash on esp32
                  RUNLOG_ARTI("%s %s %s (Push %u)\n", spaces+50-depth, key, valueStr, valueStack->stack_index);
                #endif
                break;
              default:
                valueStack->push(parseTree["token"].as<uint8_t>()); // push Operator index
                #if ARTI_PLATFORM != ARTI_ARDUINO  //for some weird reason this causes a crash on esp32
                  RUNLOG_ARTI("%s %s %s (Push %u)\n", spaces+50-depth, key, valueStr, valueStack->stack_index);
                #endif
            }
            visitedAlready = true;
          }
          else //if key is node_name
          {
            uint8_t node = stringToNode(key);

            // RUNLOG_ARTI("%s Node %s\n", spaces+50-depth, key);

            switch (node)
            {
              case F_Program: 
              {
                const char * program_name = value["ID"];
                RUNLOG_ARTI("%s program %s\n", spaces+50-depth, program_name);

                ActivationRecord* ar = new ActivationRecord(program_name, "PROGRAM", 1);

                this->callStack->push(ar);

                interpret(value["block"], nullptr, global_scope, depth + 1);

                // do not delete main stack and program ar as used in subsequent calls 
                // this->callStack->pop();
                // delete ar; ar = nullptr;

                visitedAlready = true;
                break;
              }
              case F_Function: 
              {
                const char * function_name = value["ID"];
                Symbol* function_symbol = current_scope->lookup(function_name);
                RUNLOG_ARTI("%s Save block of %s\n", spaces+50-depth, function_name);
                if (function_symbol != nullptr)
                  function_symbol->block = value["block"];
                else
                  ERROR_ARTI("%s Function %s: not found\n", spaces+50-depth, function_name); 

                visitedAlready = true;
                break;
              }
              case F_Call: 
              {
                const char * function_name = value["ID"];

                //check if external function
                if (value.containsKey("external")) {
                  uint8_t oldIndex = valueStack->stack_index;

                  if (value.containsKey("actuals"))
                    interpret(value["actuals"], nullptr, current_scope, depth + 1);

                  float returnValue = floatNull;

                  returnValue = arti_external_function(value["external"], valueStack->floatStack[oldIndex]
                                                                        , (valueStack->stack_index - oldIndex>1)?valueStack->floatStack[oldIndex+1]:floatNull
                                                                        , (valueStack->stack_index - oldIndex>2)?valueStack->floatStack[oldIndex+2]:floatNull
                                                                        , (valueStack->stack_index - oldIndex>3)?valueStack->floatStack[oldIndex+3]:floatNull
                                                                        , (valueStack->stack_index - oldIndex>4)?valueStack->floatStack[oldIndex+4]:floatNull);

                  #if ARTI_PLATFORM != ARTI_ARDUINO // because arduino runs the code instead of showing the code
                    uint8_t lastIndex = oldIndex;
                    RUNLOG_ARTI("%s Call %s(", spaces+50-depth, function_name);
                    char sep[3] = "";
                    for (int i = oldIndex; i< valueStack->stack_index; i++) {
                      RUNLOG_ARTI("%s%f", sep, valueStack->floatStack[i]);
                      strcpy(sep, ", ");
                    }
                    if ( returnValue != floatNull)
                      RUNLOG_ARTI(") = %f (Pop %u, Push %u)\n", returnValue, oldIndex, oldIndex + 1);
                    else
                      RUNLOG_ARTI(") (Pop %u)\n", oldIndex);

                  #endif

                  valueStack->stack_index = oldIndex;

                  if (returnValue != floatNull)
                    valueStack->push(returnValue);

                }
                else { //not an external function
                  Symbol* function_symbol = current_scope->lookup(function_name);

                  if (function_symbol != nullptr) //calling undefined function: pre-defined functions e.g. print
                  {
                    ActivationRecord* ar = new ActivationRecord(function_name, "Function", function_symbol->scope_level + 1);

                    RUNLOG_ARTI("%s %s %s\n", spaces+50-depth, key, function_name);

                    uint8_t oldIndex = valueStack->stack_index;
                    uint8_t lastIndex = valueStack->stack_index;

                    if (value.containsKey("actuals"))
                      interpret(value["actuals"], nullptr, current_scope, depth + 1);

                    for (uint8_t i=0; i<function_symbol->function_scope->nrOfFormals; i++)
                    {
                      //determine type, for now assume float
                      float result = valueStack->floatStack[lastIndex++];
                      ar->set(function_symbol->function_scope->symbols[i]->scope_index, result);
                      RUNLOG_ARTI("%s Actual %s.%s = %f (pop %u)\n", spaces+50-depth, function_name, function_symbol->function_scope->symbols[i]->name, result, valueStack->stack_index);
                    }

                    valueStack->stack_index = oldIndex;

                    this->callStack->push(ar);

                    interpret(function_symbol->block, nullptr, function_symbol->function_scope, depth + 1);

                    this->callStack->pop();

                    delete ar; ar =  nullptr;

                    //tbd if syntax supports returnvalue
                    // char callResult[charLength] = "CallResult tbd of ";
                    // strcat(callResult, function_name);
                    // valueStack->push(callResult);

                  } //function_symbol != nullptr
                  else {
                    RUNLOG_ARTI("%s %s not found %s\n", spaces+50-depth, key, function_name);
                  }
                } //external functions

                visitedAlready = true;
                break;
              }
              case F_VarRef:
              case F_Assign: //get or set a variable
              {
                const char * variable_name;
                uint8_t variable_level;
                uint8_t variable_index;
                uint8_t variable_external;
                JsonObject variable_indices;
                JsonObject variable_value;

                float resultValue = floatNull;

                if (node == F_Assign) 
                {
                  variable_value = value["varref"];

                  if (value.containsKey("expr")) //value assignment
                  {
                    interpret(value, "expr", current_scope, depth + 1); //value pushed

                    resultValue = valueStack->popFloat(); //result of interpret expr (but not for -- and ++ !!!!)
                  }
                }
                else
                  variable_value = value;

                variable_name = variable_value["ID"];
                variable_level = variable_value["level"];
                variable_index = variable_value["index"];
                variable_external = variable_value["external"];
                variable_indices = variable_value["indices"];

                uint8_t oldIndex = valueStack->stack_index;

                //array indices
                char indices[charLength]; //used in RUNLOG_ARTI only
                strcpy(indices, "");
                if (!variable_indices.isNull()) 
                {
                  strcat(indices, "[");

                  interpret(variable_value, "indices", current_scope, depth + 1); //values of indices pushed

                  char sep[3] = "";
                  for (uint8_t i = oldIndex; i< valueStack->stack_index; i++) {
                    strcat(indices, sep);
                    char itoaChar[charLength];
                    // itoa(valueStack->floatStack[i], itoaChar, 10);
                    snprintf(itoaChar, sizeof(itoaChar), "%f", valueStack->floatStack[i]);
                    strcat(indices, itoaChar);
                    strcpy(sep, ",");
                  }

                  strcat(indices, "]");
                }

                //check if external variable
                if (variable_value.containsKey("external")) //added by Analyze
                {

                  if (node == F_VarRef) { //get the value

                    resultValue = arti_get_external_variable(variable_external, (valueStack->stack_index - oldIndex>0)?valueStack->floatStack[oldIndex]:floatNull, (valueStack->stack_index - oldIndex>1)?valueStack->floatStack[oldIndex+1]:floatNull);
                    valueStack->stack_index = oldIndex;

                    if (resultValue != floatNull) 
                    {
                      valueStack->push(resultValue);
                      RUNLOG_ARTI("%s %s ext.%s = %f (push %u)\n", spaces+50-depth, key, variable_name, resultValue, valueStack->stack_index); //key is variable_declaration name is ID
                    }
                    else
                      ERROR_ARTI("%s Error: %s ext.%s no value\n", spaces+50-depth, key, variable_name);
                  }
                  else //assign: set the external value...
                  {
                    arti_set_external_variable(resultValue, variable_external, (valueStack->stack_index - oldIndex>0)?valueStack->floatStack[oldIndex]:floatNull, (valueStack->stack_index - oldIndex>1)?valueStack->floatStack[oldIndex+1]:floatNull);
                    valueStack->stack_index = oldIndex;

                    RUNLOG_ARTI("%s %s set ext.%s%s = %f (Pop %u)\n", spaces+50-depth, key, variable_name, indices, resultValue, oldIndex);
                  }
                }
                else //not external, get er set the variable
                {
                  // Symbol* variable_symbol = current_scope->lookup(variable_name);
                  ActivationRecord* ar;

                  //check already defined in this scope

                  // RUNLOG_ARTI("%s levels %u-%u\n", spaces+50-depth, variable_level,  variable_index );
                  if (variable_level != 0) { //var already exist
                    //calculate the index in the call stack to find the right ar
                    uint8_t index = this->callStack->recordsCounter - 1 - (this->callStack->peek()->nesting_level - variable_level);
                    //  RUNLOG_ARTI("%s %s %s.%s = %s (push) %s %d-%d = %d (%d)\n", spaces+50-depth, key, ar->name, variable_name, varValue, variable_symbol->name, this->callStack->peek()->nesting_level,variable_symbol->scope_level, index,  this->callStack->recordsCounter); //key is variable_declaration name is ID
                    ar = this->callStack->records[index];
                  }
                  else //var created here
                    ar = this->callStack->peek();

                  if (ar != nullptr) // variable found
                  {
                    if (node == F_VarRef) //get the value
                    {
                      //determine type, for now assume float
                      float varValue = ar->getFloat(variable_index);

                      valueStack->push(varValue);
                      #if ARTI_PLATFORM != ARTI_ARDUINO  //for some weird reason this causes a crash on esp32
                        RUNLOG_ARTI("%s %s %s.%s = %f (push %u) %u-%u\n", spaces+50-depth, key, ar->name, variable_name, varValue, valueStack->stack_index, variable_level, variable_index); //key is variable_declaration name is ID
                      #endif
                    }
                    else { //assign: set the value 

                      if (value.containsKey("assignoperator")) 
                      {
                        switch (value["assignoperator"].as<uint8_t>()) 
                        {
                          case F_plus:
                            ar->set(variable_index, ar->getFloat(variable_index) + resultValue);
                            break;
                          case F_minus:
                            ar->set(variable_index, ar->getFloat(variable_index) - resultValue);
                            break;
                          case F_multiplication:
                            ar->set(variable_index, ar->getFloat(variable_index) * resultValue);
                            break;
                          case F_division: 
                          {
                            if (resultValue == 0) // divisor
                            {
                              resultValue = 1;
                              ERROR_ARTI("%s /= division by 0 not possible, divisor ignored for %f\n", spaces+50-depth, ar->getFloat(variable_index));
                            }
                            ar->set(variable_index, ar->getFloat(variable_index) / resultValue);
                            break;
                          }
                          case F_plusplus:
                            ar->set(variable_index, ar->getFloat(variable_index) + 1);
                            break;
                          case F_minmin:
                            ar->set(variable_index, ar->getFloat(variable_index) - 1);
                            break;
                        }

                        RUNLOG_ARTI("%s %s.%s%s %s= %f (pop %u) %u-%u\n", spaces+50-depth, ar->name, variable_name, indices, tokenToString(value["assignoperator"]), ar->getFloat(variable_index), valueStack->stack_index, variable_level, variable_index);
                      }
                      else 
                      {
                        ar->set(variable_index, resultValue);
                        RUNLOG_ARTI("%s %s.%s%s := %f (pop %u) %u-%u\n", spaces+50-depth, ar->name, variable_name, indices, ar->getFloat(variable_index), valueStack->stack_index, variable_level, variable_index);
                      }
                      valueStack->stack_index = oldIndex;
                    }
                  } //ar != nullptr
                  else { //unknown variable
                    ERROR_ARTI("%s %s %s unknown\n", spaces+50-depth, key, variable_name);
                    valueStack->push(floatNull);
                  }
                } // ! fouund
                visitedAlready = true;
                break;
              }
              case F_Expr:
              case F_Term: 
              {
                uint8_t oldIndex = valueStack->stack_index;

                // RUNLOG_ARTI("%s before expr term interpret %s %s\n", spaces+50-depth, key, asChar(value));
                interpret(value, nullptr, current_scope, depth + 1); //pushes results

                // RUNLOG_ARTI("%s %s interpret > (%u - %u = %u)\n", spaces+50-depth, key, valueStack->stack_index, oldIndex, valueStack->stack_index - oldIndex);

                // always 3, 5, 7 ... values
                if (valueStack->stack_index - oldIndex >= 3) 
                {
                  float left = valueStack->floatStack[oldIndex];
                  for (int i = 3; i <= valueStack->stack_index - oldIndex; i += 2)
                  {
                    uint8_t operatorx = valueStack->floatStack[oldIndex + i - 2];
                    float right = valueStack->floatStack[oldIndex + i - 1];

                    float evaluation = 0;

                    switch (operatorx) {
                      case F_plus: 
                        evaluation = left + right;
                        break;
                      case F_minus: 
                        evaluation = left - right;
                        break;
                      case F_multiplication: 
                        evaluation = left * right;
                        break;
                      case F_division: {
                        if (right == 0)
                        {
                          right = 1;
                          ERROR_ARTI("%s division by 0 not possible, divisor ignored for %f\n", spaces+50-depth, left);
                        }
                        evaluation = left / right;
                        break;
                      }
                      case F_modulo: {
                        if (right == 0) {
                          evaluation = left;
                          ERROR_ARTI("%s mod 0 not possible, mod ignored %f\n", spaces+50-depth, left);
                        }
                        else 
                          evaluation = fmod(left, right);
                        break;
                      }
                      case F_bitShiftLeft: 
                        evaluation = (int)left << (int)right; //only works on integers
                        break;
                      case F_bitShiftRight: 
                        evaluation = (int)left >> (int)right; //only works on integers
                        break;
                      case F_equal: 
                        evaluation = left == right;
                        break;
                      case F_notEqual: 
                        evaluation = left != right;
                        break;
                      case F_lessThen: 
                        evaluation = left < right;
                        break;
                      case F_lessThenOrEqual: 
                        evaluation = left <= right;
                        break;
                      case F_greaterThen: 
                        evaluation = left > right;
                        break;
                      case F_greaterThenOrEqual: 
                        evaluation = left >= right;
                        break;
                      case F_and: 
                        evaluation = left && right;
                        break;
                      case F_or: 
                        evaluation = left || right;
                        break;
                      default:
                        ERROR_ARTI("%s Programming error: unknown operator %u\n", spaces+50-depth, operatorx);
                    }

                    RUNLOG_ARTI("%s %f %s %f = %f (pop %u, push %u)\n", spaces+50-depth, left, tokenToString(operatorx), right, evaluation, valueStack->stack_index - i, valueStack->stack_index - i + 1);

                    left = evaluation;
                  }

                  valueStack->stack_index = oldIndex;
    
                  valueStack->push(left);
                }
                else if (valueStack->stack_index - oldIndex == 2) // unary: operator and 1 operand
                {
                  uint8_t operatorx = valueStack->floatStack[oldIndex];
                  if (operatorx == F_minus) 
                  {
                    valueStack->stack_index = oldIndex;
                    valueStack->push(-valueStack->floatStack[oldIndex + 1]);
                    RUNLOG_ARTI("%s unary - %f (push %u)\n", spaces+50-depth, valueStack->floatStack[oldIndex + 1], valueStack->stack_index );
                  }
                  else {
                    RUNLOG_ARTI("%s unary operator not supported %u %s\n", spaces+50-depth, operatorx, tokenToString(operatorx));
                  }
                }

                visitedAlready = true;
                break;
              }
              case F_For: 
              {
                RUNLOG_ARTI("%s For (%u)\n", spaces+50-depth, valueStack->stack_index);

                interpret(value, "assign", current_scope, depth + 1); //creates the assignment
                ActivationRecord* ar = this->callStack->peek();

                bool continuex = true;
                uint16_t counter = 0;
                while (continuex && counter < 2000) //to avoid endless loops
                {
                  RUNLOG_ARTI("%s iteration\n", spaces+50-depth);

                  RUNLOG_ARTI("%s check to condition\n", spaces+50-depth);
                  interpret(value, "expr", current_scope, depth + 1); //pushes result of to

                  float conditionResult = valueStack->popFloat();

                  RUNLOG_ARTI("%s conditionResult (pop %u)\n", spaces+50-depth, valueStack->stack_index);

                  if (conditionResult == 1) { //conditionResult is true
                    RUNLOG_ARTI("%s 1 => run block\n", spaces+50-depth);
                    interpret(value["block"], nullptr, current_scope, depth + 1);

                    RUNLOG_ARTI("%s assign next value\n", spaces+50-depth);
                    interpret(value["increment"], nullptr, current_scope, depth + 1); //pushes increment result
                    // MEMORY_ARTI("%s Iteration %u %u\n", spaces+50-depth, counter, FREE_SIZE);
                  }
                  else 
                  {
                    if (conditionResult == 0) { //conditionResult is false
                      RUNLOG_ARTI("%s 0 => end of For\n", spaces+50-depth);
                      continuex = false;
                    }
                    else // conditionResult is a value (e.g. in pascal)
                    {
                      //get the variable from assignment
                      float varValue = ar->getFloat(ar->lastSetIndex);

                      float evaluation = varValue <= conditionResult;
                      RUNLOG_ARTI("%s %s.(%u) %f <= %f = %f\n", spaces+50-depth, ar->name, ar->lastSetIndex, varValue, conditionResult, evaluation);

                      if (evaluation == 1) 
                      {
                        RUNLOG_ARTI("%s 1 => run block\n", spaces+50-depth);
                        interpret(value["block"], nullptr, current_scope, depth + 1);

                        //increment
                        ar->set(ar->lastSetIndex, varValue + 1);
                      }
                      else 
                      {
                        RUNLOG_ARTI("%s 0 => end of For\n", spaces+50-depth);
                        continuex = false;
                      }
                    }
                  }
                  counter++;
                };

                if (continuex)
                  ERROR_ARTI("%s too many iterations in for loop %u\n", spaces+50-depth, counter);

                visitedAlready = true;
                break;
              }  // case
              case F_If: 
              {
                RUNLOG_ARTI("%s If (stack %u)\n", spaces+50-depth, valueStack->stack_index);

                RUNLOG_ARTI("%s condition\n", spaces+50-depth);
                if (value.containsKey("expr"))
                  interpret(value, "expr", current_scope, depth + 1);
                // else if (value.containsKey("varref"))
                //   interpret(value, "varref", current_scope, depth + 1);

                float conditionResult = valueStack->popFloat();

                RUNLOG_ARTI("%s (pop %u)\n", spaces+50-depth, valueStack->stack_index);

                if (conditionResult == 1) //conditionResult is true
                  interpret(value, "block", current_scope, depth + 1);
                else
                  interpret(value, "elseBlock", current_scope, depth + 1);

                visitedAlready = true;
                break;
              }  // case
              case F_Cex: 
              {
                RUNLOG_ARTI("%s Cex (stack %u)\n", spaces+50-depth, valueStack->stack_index);

                RUNLOG_ARTI("%s condition\n", spaces+50-depth);
                interpret(value, "expr", current_scope, depth + 1);

                float conditionResult = valueStack->popFloat();

                RUNLOG_ARTI("%s (pop %u)\n", spaces+50-depth, valueStack->stack_index);

                if (conditionResult == 1) //conditionResult is true
                  interpret(value, "trueExpr", current_scope, depth + 1);
                else
                  interpret(value, "falseExpr", current_scope, depth + 1);

                visitedAlready = true;
                break;
              }  // case
              default:  //visitedalready false => recursive call
                break;
            }
          } // is key is node_name

          if (!visitedAlready && value.size() > 0) // if size == 0 then injected key/value like operator
            interpret(value, nullptr, current_scope, depth + 1);
        } // if treeelement
                // RUNLOG_ARTI("%s before end for %u\n", spaces+50-depth, depth);
      } // for (JsonPair)
    }
    else if (parseTree.is<JsonArray>()) 
    {
      for (JsonVariant newParseTree: parseTree.as<JsonArray>()) 
      {
        // RUNLOG_ARTI("%s\n", spaces+50-depth, "Array ", parseTree[i], "  ";
        interpret(newParseTree, nullptr, current_scope, depth + 1);
      }
    }
    else { //not array
      ERROR_ARTI("%s Error: parseTree should be array or object %s (%u)\n", spaces+50-depth, asChar(parseTree), depth);
    }

    return !errorOccurred;
  } //interpret

  void closeLog() 
  {
    //non arduino stops log here
    #if ARTI_PLATFORM == ARTI_ARDUINO
      if (logToFile) 
      {
        logFile.close();
        logToFile = false;
      }
    #else
      if (logToFile)
      {
        fclose(logFile);
        logToFile = false;
      }
    #endif
  }

  bool setup(const char *definitionName, const char *programName)
  {
    errorOccurred = false;
    frameCounter = 0;

    // softhack007 check that programName has max 43 chars: fileNameLength -7 ("/" +Name + ".wled\0")
    if ((programName == NULL) || (strlen(programName) < 1) || (strlen(programName) > (fileNameLength-7))) {
      if (!logFile) logToFile = false;   // make sure this message gets to the user
      ERROR_ARTI("ARTI-FX: Invalid program name '%s'. Name must be less than %u chars.\n", programName, (unsigned)fileNameLength-7);
      return false;
    }

    logToFile = true;
    //open logFile
    if (logToFile)
    {
      #if ARTI_PLATFORM == ARTI_ARDUINO
        strcpy(logFileName, "/");
      #endif
      strcat(logFileName, programName);   // softhack007 this may overflow logFileName, in case programName has more than 44 chars
      strcat(logFileName, ".log");

      #if ARTI_PLATFORM == ARTI_ARDUINO
        logFile = WLED_FS.open(logFileName,"w");
        if (!logFile) {
          logToFile = false;
          ERROR_ARTI("ARTI-FX: Failed to create logfile '%s'\n", logFileName);
          //ERROR_ARTI("ARTI-FX: Failed to create logfile '%s': %s\n", logFileName, strerror(errno));  // unfortunately, errno is not supported on older platforms
        }
      #else
        logFile = fopen (logFileName,"w");
      #endif
    }

    MEMORY_ARTI("setup %u bytes free\n", FREE_SIZE);

    if (stages < 1) {close(); return true;}
    bool loadParseTreeFile = false;

    #if ARTI_PLATFORM == ARTI_ARDUINO
      File definitionFile;
      definitionFile = WLED_FS.open(definitionName, "r");
    #else
      std::fstream definitionFile;
      definitionFile.open(definitionName, std::ios::in);
    #endif

    MEMORY_ARTI("open %s %u ✓\n", definitionName, FREE_SIZE);

    if (!definitionFile) 
    {
      ERROR_ARTI("Definition file %s not found. Press Download wled json\n", definitionName);
      return false;
    }
    
    //open definitionFile
    #if ARTI_PLATFORM == ARTI_ARDUINO
      definitionJsonDoc = new PSRAMDynamicJsonDocument(8192); //currently 5335
    #else
      definitionJsonDoc = new PSRAMDynamicJsonDocument(16384); //currently 9521
    #endif

    // mandatory tokens:
    //  "ID": "ID",
    //  "INTEGER_CONST": "INTEGER_CONST",
    //  "REAL_CONST": "REAL_CONST",

    MEMORY_ARTI("definitionTree %u => %u ✓\n", (unsigned int)definitionJsonDoc->capacity(), FREE_SIZE); //unsigned int needed when running embedded to suppress warnings

    DeserializationError err = deserializeJson(*definitionJsonDoc, definitionFile);
    if (err) 
    {
      ERROR_ARTI("deserializeJson() of definition failed with code %s\n", err.c_str());
      return false;
    }
    definitionFile.close();
    definitionJson = definitionJsonDoc->as<JsonObject>();

    JsonObject::iterator objectIterator = definitionJson.begin();
    JsonObject metaData = objectIterator->value();
    const char * version = metaData["version"];
    if (strcmp(version, "v033") != 0) 
    {
      ERROR_ARTI("Version of definition.json file (%s) should be v033.\nPress Download wled json\n", version);
      return false;
    }
    const char * startNode = metaData["start"];
    if (startNode == nullptr) 
    {
      ERROR_ARTI("Setup Error: No start node found in definition file %s\n", definitionName);
      return false;
    }

    char programFileName[fileNameLength];
    #if ARTI_PLATFORM == ARTI_ARDUINO
      strcpy(programFileName, "/");
    #endif
    strcat(programFileName, programName);    // softhack007 this may overflow programFileName, in case programName has more than 43 chars
    strcat(programFileName, ".wled");

    #if ARTI_PLATFORM == ARTI_ARDUINO
      File programFile;
      programFile = WLED_FS.open(programFileName, "r");
    #else
      std::fstream programFile;
      programFile.open(programFileName, std::ios::in);
    #endif
    MEMORY_ARTI("open %s %u ✓\n", programFileName, FREE_SIZE);
    if (!programFile) 
    {
      ERROR_ARTI("Program file '%s' not found\n", programFileName);
      //ERROR_ARTI("Program file '%s' not found: %s\n", programFileName, strerror(errno)); // errno is not supported on older platforms
      return  false;
    }

    //open programFile
    char * programText = nullptr;
    uint16_t programFileSize;
    #if ARTI_PLATFORM == ARTI_ARDUINO
      programFileSize = programFile.size();
      programText = (char *)malloc(programFileSize+1);
      programFile.read((byte *)programText, programFileSize);
      programText[programFileSize] = '\0';
    #else
      programText = (char *)malloc(programTextSize);
      programFile.read(programText, programTextSize);
      DEBUG_ARTI("programFile size %lu bytes\n", programFile.gcount());
      programText[programFile.gcount()] = '\0';
      programFileSize = strlen(programText);
    #endif
    programFile.close();

    #if ARTI_PLATFORM == ARTI_ARDUINO
      parseTreeJsonDoc = new PSRAMDynamicJsonDocument(32768); //less memory on arduino: 32 vs 64 bit?
    #else
      parseTreeJsonDoc = new PSRAMDynamicJsonDocument(65536);
    #endif

    MEMORY_ARTI("parseTree %u => %u ✓\n", (unsigned int)parseTreeJsonDoc->capacity(), FREE_SIZE);

    //parse

    #ifdef ARTI_DEBUG // only read write file if debug is on
      char parseTreeName[fileNameLength];
      #if ARTI_PLATFORM == ARTI_ARDUINO
        strcpy(parseTreeName, "/");
      #endif
      strcat(parseTreeName, programName);
      // if (loadParseTreeFile)
      //   strcpy(parseTreeName, "Gen");
      strcat(parseTreeName, ".json");

      #if ARTI_PLATFORM == ARTI_ARDUINO
        File parseTreeFile;
        parseTreeFile = WLED_FS.open(parseTreeName, loadParseTreeFile?"r":"w");
      #else
        std::fstream parseTreeFile;
        parseTreeFile.open(parseTreeName, loadParseTreeFile?std::ios::in:std::ios::out);
      #endif
    #endif

    if (stages < 1) {
      if (nullptr != programText) free(programText);  // softhack007 prevent memory leak
      close(); 
      return true;
    }

    if (!loadParseTreeFile) 
    {
      parseTreeJson = parseTreeJsonDoc->as<JsonVariant>();

      lexer = new Lexer(programText, definitionJson);
      lexer->get_next_token();

      if (stages < 2) {
        //if (nullptr != programText) free(programText);  // softhack007 needed to prevent memory leak? lexer has a pointer to programText so its still in use maybe?
        close(); 
        return true;
      }

      uint8_t result = parse(parseTreeJson, startNode, '&', lexer->definitionJson[startNode], 0);

      if (this->lexer->pos != strlen(this->lexer->text)) 
      {
        ERROR_ARTI("Node %s Program not entirely parsed (%u,%u) %u of %u\n", startNode, this->lexer->lineno, this->lexer->column, this->lexer->pos, (unsigned int)strlen(this->lexer->text));
        //if (nullptr != programText) free(programText);  // softhack007 needed to prevent memory leak? lexer has a pointer to programText so its still in use maybe?
        return false;
      }
      else if (result == ResultFail) 
      {
        ERROR_ARTI("Node %s Program parsing failed (%u,%u) %u of %u\n", startNode, this->lexer->lineno, this->lexer->column, this->lexer->pos, (unsigned int)strlen(this->lexer->text));
        //if (nullptr != programText) free(programText);  // softhack007 needed to prevent memory leak? lexer has a pointer to programText so its still in use maybe?
        return false;
      }
      else
      {
        DEBUG_ARTI("Node %s Parsed until (%u,%u) %u of %u\n", startNode, this->lexer->lineno, this->lexer->column, this->lexer->pos, (unsigned int)strlen(this->lexer->text));
        MEMORY_ARTI("parse %u ✓\n", FREE_SIZE);
      }

      MEMORY_ARTI("definitionTree %u / %u%% (%u %u %u)\n", (unsigned int)definitionJsonDoc->memoryUsage(), 100 * definitionJsonDoc->memoryUsage() / definitionJsonDoc->capacity(), (unsigned int)definitionJsonDoc->size(), definitionJsonDoc->overflowed(), (unsigned int)definitionJsonDoc->nesting());
      MEMORY_ARTI("parseTree      %u / %u%% (%u %u %u)\n", (unsigned int)parseTreeJsonDoc->memoryUsage(), 100 * parseTreeJsonDoc->memoryUsage() / parseTreeJsonDoc->capacity(), (unsigned int)parseTreeJsonDoc->size(), parseTreeJsonDoc->overflowed(), (unsigned int)parseTreeJsonDoc->nesting());
      size_t memBefore = parseTreeJsonDoc->memoryUsage();
      parseTreeJsonDoc->garbageCollect();
      MEMORY_ARTI("garbageCollect %u / %u%% -> %u / %u%%\n", (unsigned int)memBefore, 100 * memBefore / parseTreeJsonDoc->capacity(), (unsigned int)parseTreeJsonDoc->memoryUsage(), 100 * parseTreeJsonDoc->memoryUsage() / parseTreeJsonDoc->capacity());

      delete lexer; lexer =  nullptr;
    }
    else
    {
      // read parseTree
      #ifdef ARTI_DEBUG // only write file if debug is on
        DeserializationError err = deserializeJson(*parseTreeJsonDoc, parseTreeFile);
        if (err) 
        {
          ERROR_ARTI("deserializeJson() of parseTree failed with code %s\n", err.c_str());
          return false;
        }
      #endif
    }
    #if ARTI_PLATFORM == ARTI_ARDUINO //not on windows as cause crash???
      free(programText);
    #endif

    if (stages >= 3)
    {
      DEBUG_ARTI("\nOptimizer\n");
      if (!optimize(parseTreeJson)) 
      {
        ERROR_ARTI("Optimize failed\n");
        return false;
      }
      else
        MEMORY_ARTI("optimize %u ✓\n", FREE_SIZE);

        size_t memBefore = parseTreeJsonDoc->memoryUsage();
        parseTreeJsonDoc->garbageCollect();
        MEMORY_ARTI("garbageCollect %u / %u%% -> %u / %u%%\n", (unsigned int)memBefore, 100 * memBefore / parseTreeJsonDoc->capacity(), (unsigned int)parseTreeJsonDoc->memoryUsage(), 100 * parseTreeJsonDoc->memoryUsage() / parseTreeJsonDoc->capacity());

      if (stages >= 4)
      {
        ANDBG_ARTI("\nAnalyzer\n");
        if (!analyze(parseTreeJson)) 
        {
          ERROR_ARTI("Analyze failed\n");
          errorOccurred = true;
        }
        else
          MEMORY_ARTI("analyze %u ✓\n", FREE_SIZE);
      }
    }

    size_t memBefore = parseTreeJsonDoc->memoryUsage();
    parseTreeJsonDoc->garbageCollect();
    MEMORY_ARTI("garbageCollect %u / %u%% -> %u / %u%%\n", (unsigned int)memBefore, 100 * memBefore / parseTreeJsonDoc->capacity(), (unsigned int)parseTreeJsonDoc->memoryUsage(), 100 * parseTreeJsonDoc->memoryUsage() / parseTreeJsonDoc->capacity());

    #ifdef ARTI_DEBUG // only write parseTree file if debug is on
      if (!loadParseTreeFile)
        serializeJsonPretty(*parseTreeJsonDoc,  parseTreeFile);
      parseTreeFile.close();
    #endif

    if (stages < 5 || errorOccurred) {close(); return !errorOccurred;}

    //interpret main
    callStack = new CallStack();
    valueStack = new ValueStack();

    if (global_scope != nullptr) //due to undefined functions??? wip
    { 
      RUNLOG_ARTI("\ninterpret %s %u %u\n", global_scope->scope_name, global_scope->scope_level, global_scope->symbolsIndex); 

      if (!interpret(parseTreeJson)) 
      {
        ERROR_ARTI("Interpret main failed\n");
        return false;
      }
    }
    else
    {
      ERROR_ARTI("\nInterpret global scope is nullptr\n");
      return false;
    }

    MEMORY_ARTI("Interpret main %u ✓\n", FREE_SIZE);
 
    return !errorOccurred;
  } // setup

  void close() {
    MEMORY_ARTI("closing Arti %u\n", FREE_SIZE);

    if (callStack != nullptr) {delete callStack; callStack = nullptr;}
    if (valueStack != nullptr) {delete valueStack; valueStack = nullptr;}
    if (global_scope != nullptr) {delete global_scope; global_scope = nullptr;}

    if (definitionJsonDoc != nullptr) {
      MEMORY_ARTI("definitionJson  %u / %u%% (%u %u %u)\n", (unsigned int)definitionJsonDoc->memoryUsage(), 100 * definitionJsonDoc->memoryUsage() / definitionJsonDoc->capacity(), (unsigned int)definitionJsonDoc->size(), definitionJsonDoc->overflowed(), (unsigned int)definitionJsonDoc->nesting());
      delete definitionJsonDoc; definitionJsonDoc = nullptr;
    }

    if (parseTreeJsonDoc != nullptr) {
      MEMORY_ARTI("parseTree       %u / %u%% (%u %u %u)\n", (unsigned int)parseTreeJsonDoc->memoryUsage(), 100 * parseTreeJsonDoc->memoryUsage() / parseTreeJsonDoc->capacity(), (unsigned int)parseTreeJsonDoc->size(), parseTreeJsonDoc->overflowed(), (unsigned int)parseTreeJsonDoc->nesting());
      delete parseTreeJsonDoc; parseTreeJsonDoc = nullptr;
    }

    MEMORY_ARTI("closed Arti %u ✓\n", FREE_SIZE);

    closeLog();

    #if ARTI_PLATFORM == ARTI_ARDUINO
      WLED_FS.remove(logFileName); //cleanup the /edit folder a bit
    #endif
  }
}; //ARTI