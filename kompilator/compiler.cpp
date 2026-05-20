#include "compiler.h"
#include <cstdlib>
#include <algorithm>
#include <cctype>
#include <sstream>
extern int yylineno;

Compiler::Compiler()
{
    memory_offset = 0;
    label_counter = 0;
    current_proc_name = "";
    temp_addr_loc = memory_offset++; 
    main_start_label = new_label();
    emit_jump("JUMP", main_start_label);
}

Variable *Compiler::get_var(const string &name)
{
    if (!current_proc_name.empty())
    {
        auto pit = procedures.find(current_proc_name);
        if (pit == procedures.end())
        {
            cerr << "Błąd wewnętrzny: current_proc_name = '" << current_proc_name << "' nie istnieje." << endl;
            exit(1);
        }
        Procedure &proc = pit->second;
        auto it = proc.locals.find(name);
        if (it != proc.locals.end())
            return &it->second;

        cerr << "Błąd: Użycie niezadeklarowanej zmiennej lokalnej/parametru '" << name
             << "' w procedurze " << current_proc_name << " (linia " << yylineno << ")" << endl;
        exit(1);
    }
    else
    {
        auto git = global_symbols.find(name);
        if (git != global_symbols.end())
            return &git->second;
    }
    cerr << "Błąd: Użycie niezadeklarowanej zmiennej '" << name << "' (linia " << yylineno << ")" << endl;
    exit(1);
}

long long Compiler::new_label() { return ++label_counter; }

void Compiler::declare_variable(const string &name)
{
    Variable v;
    v.address = memory_offset++;
    v.type = VAR_SIMPLE;
    v.is_reference = false;
    v.is_iterator = false;
    v.array_start = 0;
    v.array_end = 0;
    v.param_mode = "";
    v.initialized = false;
    v.virtual_address = v.address;

    if (!current_proc_name.empty())
    {
        if (procedures[current_proc_name].locals.count(name))
        {
            cerr << "Błąd: Druga deklaracja zmiennej lokalnej " << name << endl;
            exit(1);
        }
        procedures[current_proc_name].locals[name] = v;
    }
    else
    {
        if (global_symbols.count(name))
        {
            cerr << "Błąd: Druga deklaracja zmiennej globalnej " << name << endl;
            exit(1);
        }
        global_symbols[name] = v;
    }
}

void Compiler::declare_array(const string &name, long long start, long long end)
{
    if (start > end)
    {
        cerr << "Błąd: Zły zakres tablicy " << name << endl;
        exit(1);
    }
    long long size = end - start + 1;
    Variable v;
    v.address = memory_offset; 
    v.type = VAR_ARRAY;
    v.is_reference = false;
    v.is_iterator = false;
    v.array_start = start;
    v.array_end = end;
    v.param_mode = "";
    v.initialized = false;
    v.virtual_address = memory_offset - start;

    memory_offset += size;

    if (!current_proc_name.empty())
    {
        if (procedures[current_proc_name].locals.count(name))
        {
            cerr << "Błąd: Druga deklaracja tablicy lokalnej " << name << endl;
            exit(1);
        }
        procedures[current_proc_name].locals[name] = v;
    }
    else
    {
        if (global_symbols.count(name))
        {
            cerr << "Błąd: Druga deklaracja tablicy globalnej " << name << endl;
            exit(1);
        }
        global_symbols[name] = v;
    }
}

void Compiler::proc_start(const string &proc_name)
{
    if (procedures.find(proc_name) != procedures.end())
    {
        cerr << "Błąd: Druga definicja procedury " << proc_name << endl;
        exit(1);
    }

    Procedure proc;
    proc.name = proc_name;
    proc.start_label = new_label();
    proc.return_addr_loc = -1;
    auto res = procedures.emplace(proc_name, std::move(proc));

    Procedure &stored_proc = res.first->second;
    current_proc_name = proc_name;
    stored_proc.return_addr_loc = memory_offset++; 

    set_label(stored_proc.start_label);
    emit("STORE", stored_proc.return_addr_loc); 

    for (auto &arg : arg_decl_buffer) 
    {
        string type = arg.first;
        string name = arg.second;
        Variable v;
        v.address = memory_offset++; 
        v.is_reference = true;
        v.is_iterator = false;
        v.type = (type == "T") ? VAR_ARRAY : VAR_SIMPLE;
        v.array_start = 0;
        v.array_end = 0;
        v.virtual_address = v.address;
        v.param_mode = type; 
        v.initialized = (type == "O") ? false : true;
        if (stored_proc.locals.count(name))
        {
            cerr << "Błąd: Zduplikowana nazwa argumentu " << name << " w procedurze " << proc_name << endl;
            exit(1);
        }
        stored_proc.locals.emplace(name, v);
        stored_proc.params_order.push_back({name, type});
    }
    arg_decl_buffer.clear();
}

void Compiler::proc_end()
{
    auto pit = procedures.find(current_proc_name);
    if (pit == procedures.end())
    {
        cerr << "Błąd wewnętrzny: proc_end() dla nieistniejącej procedury '" << current_proc_name << "'" << endl;
        exit(1);
    }
    Procedure &proc = pit->second;

    if (proc.return_addr_loc < 0)
    {
        cerr << "Błąd wewnętrzny: return_addr_loc nie ustawiony dla procedury " << current_proc_name << endl;
        exit(1);
    }

    emit("LOAD", proc.return_addr_loc);
    emit("RTRN");
    current_proc_name.clear();
}

void Compiler::arg_decl(const string &type, const string &name)
{
    arg_decl_buffer.push_back({type, name});
}

void Compiler::arg_use(const string &arg_name)
{
    call_args_buffer.push_back(arg_name);
}

void Compiler::proc_call(const string &proc_name)
{
    auto pit = procedures.find(proc_name);
    if (pit == procedures.end())
    {
        cerr << "Błąd: Wywołanie nieznanej procedury " << proc_name << endl;
        exit(1);
    }
    Procedure &target_proc = pit->second;

    if (!current_proc_name.empty() && proc_name == current_proc_name)
    {
        cerr << "Błąd: Rekurencja bezpośrednia niedozwolona: procedura '" << proc_name
             << "' wywołuje samą siebie (linia " << yylineno << ")" << endl;
        call_args_buffer.clear();
        exit(1);
    }

    if (call_args_buffer.size() != target_proc.params_order.size())
    {
        cerr << "Błąd: Zła liczba argumentów w wywołaniu " << proc_name << endl;
        call_args_buffer.clear();
        exit(1);
    }

    for (size_t i = 0; i < call_args_buffer.size(); ++i)
    {
        string expected_type = target_proc.params_order[i].type;
        string expected_name = target_proc.params_order[i].name;
        auto dit = target_proc.locals.find(expected_name);
        if (dit == target_proc.locals.end())
        {
            cerr << "Błąd wewnętrzny: parametr '" << expected_name << "' nie istnieje w procedurze " << proc_name << endl;
            call_args_buffer.clear();
            exit(1);
        }
        Variable &dest_param = dit->second;
        string arg_var_name = call_args_buffer[i];
        Variable *src_var = get_var(arg_var_name);

        if (expected_type == "T")
        {
            if (src_var->type != VAR_ARRAY)
            {
                cerr << "Błąd: Oczekiwano tablicy jako argumentu nr " << i + 1 << " w " << proc_name
                     << " (linia " << yylineno << ")" << endl;
                call_args_buffer.clear();
                exit(1);
            }
        }
        else
        {
            if (src_var->type != VAR_SIMPLE)
            {
                cerr << "Błąd: Oczekiwano zmiennej skalarnej jako argumentu nr " << i + 1 << " w " << proc_name
                     << " (linia " << yylineno << ")" << endl;
                call_args_buffer.clear();
                exit(1);
            }
        }

        if (src_var->is_iterator && expected_type != "I")
        {
            cerr << "Błąd: iterator pętli '" << arg_var_name
                 << "' nie może być użyty jako argument pozycji in-out (argument nr "
                 << (i + 1) << ") w wywołaniu " << proc_name << " (linia " << yylineno << ")" << endl;
            call_args_buffer.clear();
            exit(1);
        }

        if (src_var->param_mode == "O" && expected_type == "I")
        {
            cerr << "Błąd: Nie można przekazać parametru oznaczonego jako O do formalnego I (argument nr "
                 << (i + 1) << " -> " << arg_var_name << ") w wywołaniu " << proc_name
                 << " (linia " << yylineno << ")" << endl;
            call_args_buffer.clear();
            exit(1);
        }

        if (expected_type == "I")
        {
            bool actual_is_param_of_caller = false;
            if (!current_proc_name.empty()) 
            {
                if (src_var->is_reference) 
                    actual_is_param_of_caller = true;
            }
            else
            {
                actual_is_param_of_caller = false;
            }

            if (actual_is_param_of_caller)
            {
                if (src_var->param_mode != "I")
                {
                    cerr << "Błąd: Formalne 'I' wymaga, aby argument nr " << (i + 1)
                         << " (" << arg_var_name << ") był oznaczony jako I w miejscu wywołania (linia "
                         << yylineno << ")." << endl;
                    call_args_buffer.clear();
                    exit(1);
                }
            }
        }

        if (src_var->is_reference) 
        {
            emit("LOAD", src_var->address); 
        }
        else
        {
            long long addr_to_pass = src_var->virtual_address;
            load_const_to_reg(addr_to_pass, 'a'); 
        }
        emit("STORE", dest_param.address); 
    }
    call_args_buffer.clear();
    emit_jump("CALL", target_proc.start_label);
}

void Compiler::program_start()
{
    set_label(main_start_label);
}

void Compiler::program_end()
{
    emit("HALT");
}

void Compiler::load_to_reg(const string &name, char target_reg)
{
    if (name.empty())
    {
        cerr << "Błąd wewnętrzny: próba załadowania pustej nazwy do rejestru" << endl;
        exit(1);
    }

    if (name[0] == '#')
    {
        long long addr = stoll(name.substr(1));
        emit("LOAD", addr);
        if (target_reg != 'a')
            emit("SWP", string(1, target_reg));
        return;
    }

    if (name[0] == '@')
    {
        emit("LOAD", temp_addr_loc); 
        emit("SWP", "b");            
        emit("RLOAD", "b");          
        if (target_reg != 'a')
            emit("SWP", string(1, target_reg));
        return;
    }

    if (isdigit((unsigned char)name[0]))
    {
        long long value = stoll(name);
        load_const_to_reg(value, target_reg);
        return;
    }

    Variable *v = get_var(name);
    if (v->param_mode == "O" && !v->initialized)
    {
        cerr << "Błąd: Odczyt zmiennej oznaczonej jako O przed przypisaniem: "
             << name << " (linia " << yylineno << ")" << endl;
        exit(1);
    }

    if (v->is_reference)
    {
        emit("LOAD", v->address);
        emit("RLOAD", "a"); 
        if (target_reg != 'a')
            emit("SWP", string(1, target_reg));
    }
    else
    {
        emit("LOAD", v->address);
        if (target_reg != 'a')
            emit("SWP", string(1, target_reg));
    }
}

void Compiler::read(const string &name)
{
    if (name.empty())
    {
        cerr << "Błąd wewnętrzny: read pusty name" << endl;
        exit(1);
    }

    if (name[0] == '@')
    {
        emit("READ");                
        emit("SWP", "b");            
        emit("LOAD", temp_addr_loc); 
        emit("SWP", "b");            
        emit("RSTORE", "b");         
        return;
    }

    Variable *v = get_var(name);
    emit("READ");

    if (v->param_mode == "I")
    {
        cerr << "Błąd: Nie wolno wykonać READ do parametru oznaczonego jako I: " << name
             << " (linia " << yylineno << ")" << endl;
        exit(1);
    }

    if (v->is_reference)
    {
        emit("SWP", "b");
        emit("LOAD", v->address);
        emit("SWP", "b");
        emit("RSTORE", "b");
        v->initialized = true;
    }
    else
    {
        emit("STORE", v->address);
        v->initialized = true;
    }
}

void Compiler::write(const string &val)
{
    load_to_reg(val, 'a');
    emit("WRITE");
}

void Compiler::array_access_var_index(const string &array_name, const string &index_var)
{
    Variable *arr = get_var(array_name);
    if (!index_var.empty() && isdigit((unsigned char)index_var[0]))
    {
        long long idx = stoll(index_var);
        if (idx < arr->array_start || idx > arr->array_end)
        {
            cerr << "Błąd: Indeks spoza zakresu dla tablicy " << array_name
                 << ": " << idx << " nie mieści się w [" << arr->array_start
                 << ":" << arr->array_end << "] (linia " << yylineno << ")" << endl;
            exit(1);
        }
    }

    load_to_reg(index_var, 'a');
    emit("SWP", "b"); 

    if (arr->is_reference)
        emit("LOAD", arr->address); 
    else
        load_const_to_reg(arr->virtual_address, 'a'); 

    emit("ADD", "b");             
    emit("STORE", temp_addr_loc); 
    emit("SWP", "b");             
    emit("RLOAD", "b");           
}

void Compiler::array_access_const_index(const string &array_name, long long index_const)
{
    Variable *arr = get_var(array_name);
    if (index_const < arr->array_start || index_const > arr->array_end)
    {
        cerr << "Błąd: Indeks spoza zakresu dla tablicy " << array_name
             << ": " << index_const << " nie mieści się w [" << arr->array_start
             << ":" << arr->array_end << "] (linia " << yylineno << ")" << endl;
        exit(1);
    }

    if (arr->is_reference)
    {
        emit("LOAD", arr->address);              
        emit("SWP", "b");                        
        load_const_to_reg(index_const, 'a'); 
        emit("ADD", "b");                        
    }
    else
    {
        load_const_to_reg(arr->virtual_address + index_const, 'a');
    }

    emit("STORE", temp_addr_loc);
    emit("SWP", "b");
    emit("RLOAD", "b"); 
}

void Compiler::prepare_array_assign_var_index(const string &array_name, const string &index_var)
{
    Variable *arr = get_var(array_name);
    if (!index_var.empty() && isdigit((unsigned char)index_var[0]))
    {
        long long idx = stoll(index_var);
        if (idx < arr->array_start || idx > arr->array_end)
        {
            cerr << "Błąd: Indeks spoza zakresu dla tablicy " << array_name
                 << ": " << idx << " nie mieści się w [" << arr->array_start
                 << ":" << arr->array_end << "] (linia " << yylineno << ")" << endl;
            exit(1);
        }
    }

    load_to_reg(index_var, 'a');
    emit("SWP", "b"); 
    if (arr->is_reference)
    {
        emit("LOAD", arr->address); 
    }
    else
    {
        load_const_to_reg(arr->virtual_address, 'a');
    }

    emit("ADD", "b"); 
    long long slot = memory_offset++;
    emit("STORE", slot); 
    assign_addr_stack.push_back(slot);
}

void Compiler::prepare_array_assign_const_index(const string &array_name, long long index_const)
{
    Variable *arr = get_var(array_name);
    if (index_const < arr->array_start || index_const > arr->array_end)
    {
        cerr << "Błąd: Indeks spoza zakresu dla tablicy " << array_name
             << ": " << index_const << " nie mieści się w [" << arr->array_start
             << ":" << arr->array_end << "] (linia " << yylineno << ")" << endl;
        exit(1);
    }
    if (arr->is_reference)
    {
        emit("LOAD", arr->address);              
        emit("SWP", "b");                        
        load_const_to_reg(index_const, 'a'); 
        emit("ADD", "b");                        
    }
    else
    {
        load_const_to_reg(arr->virtual_address + index_const, 'a');
    }

    long long slot = memory_offset++;
    emit("STORE", slot);
    assign_addr_stack.push_back(slot);
}

void Compiler::load_value(const string &val_raw)
{
    if (!val_raw.empty() && val_raw[0] == '#')
    {
        load_to_reg(val_raw, 'a');
        return;
    }
    load_to_reg(val_raw, 'a');
}

long long Compiler::capture_array_value_to_temp()
{
    long long tmp = memory_offset++;
    emit("STORE", tmp);
    return tmp;
}

void Compiler::assign_var(const string &target_name)
{
    if (target_name.empty())
    {
        cerr << "Błąd wewnętrzny: assign_var pusty target" << endl;
        exit(1);
    }

    if (target_name[0] == '@')
    {
        if (assign_addr_stack.empty())
        {
            cerr << "Błąd wewnętrzny: brak przygotowanego adresu do przypisania (\"@\")" << endl;
            exit(1);
        }

        long long addr_slot = assign_addr_stack.back();
        assign_addr_stack.pop_back();
        emit("SWP", "b");        
        emit("LOAD", addr_slot); 
        emit("SWP", "b");        
        emit("RSTORE", "b");     
        return;
    }

    Variable *v = get_var(target_name);
    if (v->is_iterator)
    {
        cerr << "Błąd: Modyfikacja iteratora pętli FOR " << target_name
             << " (linia " << yylineno << ")" << endl;
        exit(1);
    }

    if (v->param_mode == "I")
    {
        cerr << "Błąd: Modyfikacja parametru oznaczonego jako I jest zabroniona: " << target_name
             << " (linia " << yylineno << ")" << endl;
        exit(1);
    }

    if (v->is_reference)
    {
        emit("SWP", "b");         
        emit("LOAD", v->address); 
        emit("SWP", "b");         
        emit("RSTORE", "b");      
        v->initialized = true;    
    }
    else
    {
        emit("STORE", v->address);
        v->initialized = true;
    }
}

void Compiler::add(const string &left, const string &right)
{
    long long tmp = memory_offset++;
    load_to_reg(left, 'a');  
    emit("STORE", tmp);          
    load_to_reg(right, 'a'); 
    emit("SWP", "b");            
    emit("LOAD", tmp);           
    emit("ADD", "b");            
}

void Compiler::sub(const string &left, const string &right)
{
    long long tmp = memory_offset++;
    load_to_reg(left, 'a');
    emit("STORE", tmp);
    load_to_reg(right, 'a');
    emit("SWP", "b");
    emit("LOAD", tmp);
    emit("SUB", "b");
}

void Compiler::mul(const string &left, const string &right)
{
    long long l_start = new_label();
    long long l_skip_add = new_label();
    long long l_end = new_label();

    emit("RST", "e");
    emit("RST", "f");
    load_to_reg(left, 'a'); 
    emit("SWP", "b");
    load_to_reg(right, 'a'); 
    emit("SWP", "c");
    emit("RST", "d"); 

    set_label(l_start);

    emit("RST", "a");
    emit("ADD", "c");
    emit_jump("JZERO", l_end);

    emit("RST", "a");
    emit("ADD", "c");
    emit("SHR", "a");
    emit("SHL", "a");
    emit("SWP", "e"); 
    emit("RST", "a");
    emit("ADD", "c");
    emit("SUB", "e");               
    emit_jump("JZERO", l_skip_add); 

    emit("RST", "a");
    emit("ADD", "d");
    emit("ADD", "b");
    emit("SWP", "d");
    set_label(l_skip_add);

    emit("SHL", "b"); 
    emit("SHR", "c"); 
    emit_jump("JUMP", l_start);

    set_label(l_end);
    emit("SWP", "d"); 
}

void Compiler::div(const string &left, const string &right)
{
    long long l_zero_div = new_label();
    long long l_double_start = new_label();
    long long l_double_do = new_label();
    long long l_loop_start = new_label();
    long long l_do_sub = new_label();
    long long l_after_compare = new_label();
    long long l_end = new_label();

    emit("RST", "e");
    load_to_reg(left, 'b');
    load_to_reg(right, 'c');
    emit("RST", "d");

    emit("RST", "a");
    emit("ADD", "c");
    emit_jump("JZERO", l_zero_div);

    load_to_reg(right, 'e'); 
    emit("RST", "f");
    emit("INC", "f"); 

    set_label(l_double_start);
    emit("RST", "a");
    emit("ADD", "e");
    emit("SUB", "b");
    emit_jump("JZERO", l_double_do); 
    emit("SHR", "e");                
    emit("SHR", "f");
    emit_jump("JUMP", l_loop_start);

    set_label(l_double_do); 
    emit("SHL", "e");
    emit("SHL", "f");
    emit_jump("JUMP", l_double_start);

    set_label(l_loop_start); 
    emit("RST", "a");
    emit("ADD", "f");
    emit_jump("JZERO", l_end); 

    emit("RST", "a");
    emit("ADD", "b");
    emit("SUB", "e");
    emit_jump("JPOS", l_do_sub); 

    emit("RST", "a");
    emit("ADD", "e");
    emit("SUB", "b");
    emit_jump("JZERO", l_do_sub); 

    emit_jump("JUMP", l_after_compare); 

    set_label(l_do_sub);
    emit("RST", "a");
    emit("ADD", "b");
    emit("SUB", "e");
    emit("SWP", "b"); 

    emit("RST", "a");
    emit("ADD", "d");
    emit("ADD", "f");
    emit("SWP", "d"); 

    set_label(l_after_compare);
    emit("SHR", "e");
    emit("SHR", "f");
    emit_jump("JUMP", l_loop_start);

    set_label(l_zero_div);
    emit("RST", "d");

    set_label(l_end);
    emit("RST", "a");
    emit("ADD", "d"); 
}

void Compiler::mod(const string &left, const string &right)
{
    long long l_zero_div = new_label();
    long long l_double_start = new_label();
    long long l_double_do = new_label();
    long long l_loop_start = new_label();
    long long l_do_sub = new_label();
    long long l_after_compare = new_label();
    long long l_end = new_label();

    emit("RST", "e");
    load_to_reg(left, 'b');
    load_to_reg(right, 'c');
    emit("RST", "a");
    emit("ADD", "c");
    emit_jump("JZERO", l_zero_div);

    load_to_reg(right, 'e');
    emit("RST", "f");
    emit("INC", "f");

    set_label(l_double_start);
    emit("RST", "a");
    emit("ADD", "e");
    emit("SUB", "b");
    emit_jump("JZERO", l_double_do);

    emit("SHR", "e");
    emit("SHR", "f");
    emit_jump("JUMP", l_loop_start);

    set_label(l_double_do);
    emit("SHL", "e");
    emit("SHL", "f");
    emit_jump("JUMP", l_double_start);

    set_label(l_loop_start);
    emit("RST", "a");
    emit("ADD", "f");
    emit_jump("JZERO", l_end);

    emit("RST", "a");
    emit("ADD", "b");
    emit("SUB", "e");
    emit_jump("JPOS", l_do_sub);

    emit("RST", "a");
    emit("ADD", "e");
    emit("SUB", "b");
    emit_jump("JZERO", l_do_sub);

    emit_jump("JUMP", l_after_compare);

    set_label(l_do_sub);
    emit("RST", "a");
    emit("ADD", "b");
    emit("SUB", "e");
    emit("SWP", "b");

    set_label(l_after_compare);
    emit("SHR", "e");
    emit("SHR", "f");
    emit_jump("JUMP", l_loop_start);

    set_label(l_zero_div);
    emit("RST", "b");

    set_label(l_end);
    emit("RST", "a");
    emit("ADD", "b");
}

void Compiler::finish(ofstream &outfile)
{
    for (const auto &instr : program)
    {
        if (instr.jump_to_label != -1)
        {
            if (label_to_line.find(instr.jump_to_label) == label_to_line.end())
            {
                outfile << instr.op << " " << instr.jump_to_label << " # UNRESOLVED" << endl;
            }
            else
            {
                outfile << instr.op << " " << label_to_line[instr.jump_to_label] << endl;
            }
        }
        else
        {
            if (instr.arg.empty())
                outfile << instr.op << endl;
            else
                outfile << instr.op << " " << instr.arg << endl;
        }
    }
}

void Compiler::load_const_to_reg(long long val, char reg)
{
    string r(1, reg);
    emit("RST", r);
    if (val == 0)
        return;
    string bin = "";
    long long temp = val;
    while (temp > 0)
    {
        bin += (temp % 2 ? "1" : "0");
        temp /= 2;
    }
    reverse(bin.begin(), bin.end());

    bool first = true;
    for (char c : bin)
    {
        if (!first)
            emit("SHL", r);
        if (c == '1')
            emit("INC", r);
        first = false;
    }
}

void Compiler::emit(string command)
{
    Instruction i;
    i.op = command;
    program.push_back(i);
}

void Compiler::emit(string command, string reg)
{
    Instruction i;
    i.op = command;
    i.arg = reg;
    program.push_back(i);
}

void Compiler::emit(string command, long long addr)
{
    Instruction i;
    i.op = command;
    i.arg = to_string(addr);
    program.push_back(i);
}

void Compiler::emit_jump(string op, long long label_id)
{
    Instruction i;
    i.op = op;
    i.jump_to_label = label_id;
    program.push_back(i);
}

void Compiler::set_label(long long label_id) { label_to_line[label_id] = program.size(); }

void Compiler::condition(const string &op, const string &l, const string &r)
{
    string enc = op + "|" + l + "|" + r;
    cond_stack.push_back(enc);
}

static void split_enc(const string &enc, string &op, string &l, string &r)
{
    size_t p1 = enc.find('|');
    size_t p2 = (p1 == string::npos) ? string::npos : enc.find('|', p1 + 1);
    if (p1 == string::npos || p2 == string::npos)
    {
        cerr << "Błąd wewnętrzny: niepoprawny warunek: " << enc << endl;
        exit(1);
    }
    op = enc.substr(0, p1);
    l = enc.substr(p1 + 1, p2 - (p1 + 1));
    r = enc.substr(p2 + 1);
}

void Compiler::if_start()
{
    if (cond_stack.empty())
    {
        cerr << "Błąd wewnętrzny: IF bez condition" << endl;
        exit(1);
    }
    string enc = cond_stack.back();
    cond_stack.pop_back();
    string op, l, r;
    split_enc(enc, op, l, r);

    long long label_false = new_label();
    if_else_stack.push_back(label_false);

    long long tmpL = memory_offset++;
    long long tmpR = memory_offset++;

    load_to_reg(l, 'a');
    emit("STORE", tmpL);
    load_to_reg(r, 'a');
    emit("STORE", tmpR);

    auto emit_sub_and_jpos = [&](long long tL, long long tR, long long lbl)
    {
        emit("LOAD", tR); 
        emit("SWP", "b"); 
        emit("LOAD", tL); 
        emit("SUB", "b"); 
        emit_jump("JPOS", lbl);
    };

    if (op == "EQ")
    {
        emit_sub_and_jpos(tmpL, tmpR, label_false);
        emit_sub_and_jpos(tmpR, tmpL, label_false);
    }
    else if (op == "NEQ")
    {
        long long cont = new_label();
        emit_sub_and_jpos(tmpL, tmpR, cont);
        emit_sub_and_jpos(tmpR, tmpL, cont);
        emit_jump("JUMP", label_false); 
        set_label(cont);
    }
    else if (op == "GT")
    {
        long long cont = new_label();
        emit_sub_and_jpos(tmpL, tmpR, cont); 
        emit_jump("JUMP", label_false);      
        set_label(cont);
    }
    else if (op == "LT")
    {
        long long cont = new_label();
        emit_sub_and_jpos(tmpR, tmpL, cont);
        emit_jump("JUMP", label_false);
        set_label(cont);
    }
    else if (op == "GEQ")
    {
        emit_sub_and_jpos(tmpR, tmpL, label_false);
    }
    else if (op == "LEQ")
    {
        emit_sub_and_jpos(tmpL, tmpR, label_false);
    }
    else
    {
        cerr << "Błąd: nieznany operator warunku: " << op << endl;
        exit(1);
    }
}

void Compiler::if_else()
{
    if (if_else_stack.empty())
    {
        cerr << "Błąd wewnętrzny: ELSE bez IF" << endl;
        exit(1);
    }
    long long label_end = new_label();
    emit_jump("JUMP", label_end); 
    long long label_false = if_else_stack.back(); 
    if_else_stack.pop_back();
    set_label(label_false);
    if_end_stack.push_back(label_end); 
}

void Compiler::if_end()
{
    if (!if_end_stack.empty())
    {
        long long lab = if_end_stack.back();
        if_end_stack.pop_back();
        set_label(lab);
    }
    else if (!if_else_stack.empty())
    {
        long long lab = if_else_stack.back();
        if_else_stack.pop_back();
        set_label(lab);
    }
    else
    {
        cerr << "Błąd wewnętrzny: ENDIF bez pasującego IF/ELSE" << endl;
        exit(1);
    }
}

void Compiler::while_start()
{
    if (cond_stack.empty())
    {
        cerr << "Błąd wewnętrzny: WHILE bez condition" << endl;
        exit(1);
    }
    string enc = cond_stack.back();
    cond_stack.pop_back();
    string op, l, r;
    split_enc(enc, op, l, r);

    long long label_start = new_label();
    long long label_end = new_label();
    set_label(label_start);

    if (op == "EQ")
    {
        load_to_reg(r, 'b');
        load_to_reg(l, 'a');
        emit("SUB", "b");
        emit_jump("JPOS", label_end);
        load_to_reg(l, 'b');
        load_to_reg(r, 'a');
        emit("SUB", "b");
        emit_jump("JPOS", label_end);
    }
    else if (op == "NEQ")
    {
        long long cont = new_label();
        load_to_reg(r, 'b');
        load_to_reg(l, 'a');
        emit("SUB", "b");
        emit_jump("JPOS", cont);
        load_to_reg(l, 'b');
        load_to_reg(r, 'a');
        emit("SUB", "b");
        emit_jump("JPOS", cont);
        emit_jump("JUMP", label_end);
        set_label(cont);
    }
    else if (op == "GT")
    {
        long long cont = new_label();
        load_to_reg(r, 'b');
        load_to_reg(l, 'a');
        emit("SUB", "b");
        emit_jump("JPOS", cont);
        emit_jump("JUMP", label_end);
        set_label(cont);
    }
    else if (op == "LT")
    {
        long long cont = new_label();
        load_to_reg(l, 'b');
        load_to_reg(r, 'a');
        emit("SUB", "b");
        emit_jump("JPOS", cont);
        emit_jump("JUMP", label_end);
        set_label(cont);
    }
    else if (op == "GEQ")
    {
        load_to_reg(l, 'b');
        load_to_reg(r, 'a');
        emit("SUB", "b");
        emit_jump("JPOS", label_end);
    }
    else if (op == "LEQ")
    {
        load_to_reg(r, 'b');
        load_to_reg(l, 'a');
        emit("SUB", "b");
        emit_jump("JPOS", label_end);
    }
    else
    {
        cerr << "Błąd: nieznany operator warunku: " << op << endl;
        exit(1);
    }

    while_stack.push_back({label_start, label_end});
}

void Compiler::while_end()
{
    if (while_stack.empty())
    {
        cerr << "Błąd wewnętrzny: ENDWHILE bez WHILE" << endl;
        exit(1);
    }
    auto pr = while_stack.back();
    while_stack.pop_back();
    long long label_start = pr.first, label_end = pr.second;
    emit_jump("JUMP", label_start);
    set_label(label_end);
}

void Compiler::repeat_start()
{
    long long lab = new_label();
    set_label(lab);
    repeat_stack.push_back({lab, -1});
}

void Compiler::repeat_end()
{
    if (repeat_stack.empty())
    {
        cerr << "Błąd wewnętrzny: UNTIL bez REPEAT" << endl;
        exit(1);
    }
    auto pr = repeat_stack.back();
    repeat_stack.pop_back();
    long long label_start = pr.first;

    if (cond_stack.empty())
    {
        cerr << "Błąd wewnętrzny: UNTIL bez warunku" << endl;
        exit(1);
    }
    string enc = cond_stack.back();
    cond_stack.pop_back();
    string op, l, r;
    split_enc(enc, op, l, r);

    long long tmpL = memory_offset++;
    long long tmpR = memory_offset++;
    load_to_reg(l, 'a');
    emit("STORE", tmpL);
    load_to_reg(r, 'a');
    emit("STORE", tmpR);

    auto emit_sub_and_jpos = [&](long long tL, long long tR, long long lbl)
    {
        emit("LOAD", tR);
        emit("SWP", "b");
        emit("LOAD", tL);
        emit("SUB", "b");
        emit_jump("JPOS", lbl);
    };

    if (op == "EQ")
    {
        emit_sub_and_jpos(tmpL, tmpR, label_start);
        emit_sub_and_jpos(tmpR, tmpL, label_start);
    }
    else if (op == "NEQ")
    {
        long long cont = new_label();
        emit_sub_and_jpos(tmpL, tmpR, cont);
        emit_sub_and_jpos(tmpR, tmpL, cont);
        emit_jump("JUMP", label_start);
        set_label(cont);
    }
    else if (op == "GT")
    {
        long long cont = new_label();
        emit_sub_and_jpos(tmpL, tmpR, cont);
        emit_jump("JUMP", label_start);
        set_label(cont);
    }
    else if (op == "LT")
    {
        long long cont = new_label();
        emit_sub_and_jpos(tmpR, tmpL, cont);
        emit_jump("JUMP", label_start);
        set_label(cont);
    }
    else if (op == "GEQ")
    {
        emit_sub_and_jpos(tmpR, tmpL, label_start);
    }
    else if (op == "LEQ")
    {
        emit_sub_and_jpos(tmpL, tmpR, label_start);
    }
    else
    {
        cerr << "Błąd: nieznany operator w UNTIL: " << op << endl;
        exit(1);
    }
}

void Compiler::for_start(const string &iter, const string &from, const string &to, const string &dir)
{
    bool created_new = false;
    bool prev_is_iter = false;
    Variable *it = nullptr;

    if (!current_proc_name.empty())
    {
        auto &proc = procedures[current_proc_name];
        auto itloc = proc.locals.find(iter);
        if (itloc != proc.locals.end())
        {
            it = &itloc->second;
            prev_is_iter = it->is_iterator;
        }
    }
    else
    {
        auto git = global_symbols.find(iter);
        if (git != global_symbols.end())
        {
            it = &git->second;
            prev_is_iter = it->is_iterator;
        }
    }

    if (it == nullptr)
    {
        declare_variable(iter);
        created_new = true;
        if (!current_proc_name.empty())
        {
            it = &procedures[current_proc_name].locals[iter];
        }
        else
        {
            it = &global_symbols[iter];
        }
    }

    if (it)
        it->is_iterator = true;

    for_created_stack.push_back(created_new);
    for_prev_iter_flag.push_back(prev_is_iter);

    long long end_loc = memory_offset++;
    load_to_reg(from, 'a'); 
    emit("STORE", it->address);
    load_to_reg(to, 'a');
    emit("STORE", end_loc);
    long long label_start = new_label();
    long long label_end = new_label();
    set_label(label_start);

    if (dir == "TO")
    {
        emit("LOAD", end_loc);     
        emit("SWP", "b");          
        emit("LOAD", it->address); 
        emit("SUB", "b");          
        emit_jump("JPOS", label_end); 
    }
    else
    {
        emit("LOAD", it->address);
        emit("SWP", "b");
        emit("LOAD", end_loc);
        emit("SUB", "b");
        emit_jump("JPOS", label_end);
    }

    for_stack.push_back({label_start, label_end});
    for_iter_stack.push_back(iter);
    for_dir_stack.push_back(dir);
    for_end_loc.push_back(end_loc);
}

void Compiler::for_end()
{
    if (for_stack.empty() || for_iter_stack.empty() || for_dir_stack.empty() || for_end_loc.empty() || for_created_stack.empty() || for_prev_iter_flag.empty())
    {
        cerr << "Błąd wewnętrzny: ENDFOR bez FOR" << endl;
        exit(1);
    }

    long long label_start = for_stack.back().first;
    long long label_end = for_stack.back().second;
    string iter_name = for_iter_stack.back();
    string dir = for_dir_stack.back();
    long long end_loc = for_end_loc.back();
    bool created_new = for_created_stack.back();
    bool prev_is_iter = for_prev_iter_flag.back();

    for_stack.pop_back();
    for_iter_stack.pop_back();
    for_dir_stack.pop_back();
    for_end_loc.pop_back();
    for_created_stack.pop_back();
    for_prev_iter_flag.pop_back();

    Variable *it = nullptr;
    if (!current_proc_name.empty())
    {
        auto &proc = procedures[current_proc_name];
        auto itloc = proc.locals.find(iter_name);
        if (itloc != proc.locals.end())
            it = &itloc->second;
    }
    else
    {
        auto git = global_symbols.find(iter_name);
        if (git != global_symbols.end())
            it = &git->second;
    }

    if (!it)
    {
        cerr << "Błąd wewnętrzny: iterator FOR nie istnieje: " << iter_name << endl;
        exit(1);
    }

    if (dir == "TO")
    {
        emit("LOAD", it->address); 
        emit("INC", "a");          
        emit("STORE", it->address);
        emit("LOAD", end_loc);
        emit("SWP", "b");
        emit("LOAD", it->address);
        emit("SUB", "b");
        emit_jump("JPOS", label_end); 
        emit_jump("JUMP", label_start);
    }
    else
    {
        emit("LOAD", it->address);
        emit_jump("JZERO", label_end);
        emit("DEC", "a"); 
        emit("STORE", it->address);
        emit("SWP", "b");      
        emit("LOAD", end_loc); 
        emit("SUB", "b");      
        emit_jump("JPOS", label_end); 
        emit_jump("JUMP", label_start);
    }

    set_label(label_end);

    if (created_new)
    {
        if (!current_proc_name.empty())
        {
            procedures[current_proc_name].locals.erase(iter_name);
        }
        else
        {
            global_symbols.erase(iter_name);
        }
    }
    else
    {
        it->is_iterator = prev_is_iter;
    }
}
