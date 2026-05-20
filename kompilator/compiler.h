#ifndef COMPILER_H
#define COMPILER_H

#include <string>
#include <vector>
#include <map>
#include <iostream>
#include <fstream>

using namespace std;

enum SymbolType
{
    VAR_SIMPLE, 
    VAR_ARRAY   
};

struct Variable
{
    long long address; 
    SymbolType type;
    bool is_reference; 
    bool is_iterator;
    long long array_start;
    long long array_end;
    long long virtual_address; 
    string param_mode; 
    bool initialized;
};

struct Param
{
    string name;
    string type;
};

struct Procedure
{
    string name;
    long long start_label;
    long long return_addr_loc;
    vector<Param> params_order;
    map<string, Variable> locals;
};

struct Instruction
{
    string op;
    string arg;
    long long label_id = -1;
    long long jump_to_label = -1;
};

class Compiler
{
private:
    long long temp_addr_loc; 
    vector<Instruction> program; 
    map<long long, long long> label_to_line; 
    map<string, Variable> global_symbols;
    long long memory_offset;
    map<string, Procedure> procedures;
    string current_proc_name; 
    vector<pair<string, string>> arg_decl_buffer; 
    vector<string> call_args_buffer; 
    long long main_start_label;

    long long label_counter;
    vector<long long> if_end_stack;
    vector<long long> if_else_stack;
    vector<pair<long long, long long>> while_stack;
    vector<pair<long long, long long>> for_stack;
    vector<string> for_iter_stack; 
    vector<string> for_dir_stack;  
    vector<long long> for_end_loc; 
    vector<pair<long long, long long>> repeat_stack;
    vector<string> cond_stack;
    vector<bool> for_created_stack;  
    vector<bool> for_prev_iter_flag; 

    long long new_label();
    Variable *get_var(const string &name); 

public:
    Compiler();
    std::vector<long long> assign_addr_stack;

    void program_start();
    void program_end();

    void proc_start(const string &proc_name);
    void proc_end();
    void arg_decl(const string &type, const string &name);
    void proc_call(const string &name);
    void arg_use(const string &arg_name); 

    void declare_variable(const string &name);
    void declare_array(const string &name, long long start, long long end);

    void read(const string &name);
    void write(const string &val);

    void prepare_array_assign_var_index(const std::string &array_name, const std::string &index_var);
    void prepare_array_assign_const_index(const std::string &array_name, long long index_const);
    long long capture_array_value_to_temp();
    void array_access_var_index(const string &array_name, const string &index_var);
    void array_access_const_index(const string &array_name, long long index_const);

    void add(const string &l, const string &r);
    void sub(const string &l, const string &r);
    void mul(const string &l, const string &r);
    void div(const string &l, const string &r);
    void mod(const string &l, const string &r);

    void condition(const string &op, const string &l, const string &r);
    void if_start();
    void if_else();
    void if_end();
    void while_start();
    void while_end();
    void repeat_start();
    void repeat_end();
    void for_start(const string &iter, const string &from, const string &to, const string &dir);
    void for_end();

    void emit(string command);
    void emit(string command, string reg);
    void emit(string command, long long addr);
    void emit_jump(string op, long long label_id);
    void set_label(long long label_id);
    void finish(ofstream &outfile);

    void load_to_reg(const string &val_raw, char reg);
    void load_const_to_reg(long long val, char reg);
    void assign_var(const string &target_name);
    void load_value(const string &val_raw);
};

#endif