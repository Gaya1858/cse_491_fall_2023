#pragma once

#include "Language.hpp"

#include "core/WorldBase.hpp"
#include "core/EasyLogging.hpp"

#include <algorithm>
#include <functional>
#include <map>
#include <stack>
#include <variant>

#include "core/AgentBase.hpp"
#include "Interfaces/TrashInterface.hpp"

using cse491::AgentBase;
using clogged::Logger;
using clogged::Team;
using clogged::LogLevel;

/// Namespace for scripting language stuff
namespace worldlang {
	
	/// @brief Class that manages program execution.
	/// 
	/// Handles run-time state of program and contains the interpreter function.
	/// 
	/// Native C++ functions can be registered on this object to extend the
	/// functionality.
	class ProgramExecutor {
	// Internal types
	public:
		/// This is the signature interpreter functions should have.
		/// 
		/// To read the arguments passed to the function, use ProgramExecutor::popArgs().
		/// To return values from a function, use ProgramExecutor::pushStack().
		using Callable = std::function<void(ProgramExecutor&)>;
		
		struct Identifier : std::string {}; 
		
		/// Variant type containing all possible values types for variables.
		using Value = std::variant < double, std::string, Callable, Identifier >;
		
	// Execution state
	private:
		/// A map of loaded files to parsed programs
		std::map<std::string, std::vector<Unit>> scripts;
		
		/// Executable code units (set by run(), etc.)
		std::vector<Unit>* code;
		
		/// Program counter
		size_t index;
		
		/// Variables
		std::map < std::string, Value > variables{};
		
		/// Execution arguments stack
		std::stack < Value > stack{};
		
		/// Call/scope stack
		std::stack < std::vector< Value > > call_stack{};
		
		/// Error message
		std::string error_message{};
		
	// Public methods
	public:
		/// Constructor
		ProgramExecutor(){
			// if function
			// skips a block if value is false
			registerFunction("if", [this](ProgramExecutor& pe){
				auto args = pe.popArgs();
				if (args.size() != 1) { error("Wrong number of arguments!"); return; }
				// jump to end of block if false
				
				if (pe.as<double>(args.at(0)) == 0){
					skipBlock();
				} else {
					// advance to start of block automatically
				}
				// mark type of block entered
				call_stack.push({"__IF_BLOCK"});
			});
			// for function
			registerFunction("for", [this](ProgramExecutor& pe){
				auto args = pe.popArgs();
				if (args.size() != 3 && args.size() != 4) { error("Wrong number of arguments!"); return; }
				auto var = pe.as<Identifier>(args.at(0));
				auto start = pe.as<double>(args.at(1));
				auto end = pe.as<double>(args.at(2));
				auto step = args.size() == 4 ? pe.as<double>(args.at(3)) : 1.0;
				
				variables.insert_or_assign(static_cast<std::string>(var), start);
				
				auto value = pe.as<double>(args.at(0));
				if ((start < end && step > 0 && value <= end)
					|| (start >= end && step < 0 && value >= end)){
					// enter the loop if conditions are met
					call_stack.push({"__FOR_BLOCK", var, start, end, step, static_cast<double>(index)});
					index++; // skip start_block which checks condition + increments
				} else {
					// skip loop entirely
					skipBlock();
					index++; // skip end_block or else it tries to jump to beginning
				}
				
			});
		}
		
		/// Constructor with function registration
		ProgramExecutor(cse491::WorldBase& world) : ProgramExecutor(){
			// Load world grid from file
			registerFunction("loadWorld", [this, &world](ProgramExecutor& pe){
				auto args = pe.popArgs();
				if (args.size() != 1) { error("Wrong number of arguments!"); return; }
				world.GetGrid().Read(as<std::string>(args.at(0)), world.GetCellTypes());
			});
			// Get the size of the world
			registerFunction("getWorldSize", [this, &world](ProgramExecutor& pe){
				auto args = pe.popArgs();
				if (args.size() != 0) { error("Wrong number of arguments!"); return; }
				pe.pushStack(static_cast<double>(world.GetGrid().GetWidth()));
				pe.pushStack(static_cast<double>(world.GetGrid().GetHeight()));
			});
			// Create an agent
			registerFunction("addAgent", [this, &world](ProgramExecutor& pe){
				auto args = pe.popArgs();
				if (args.size() < 5) { error("Wrong number of arguments!"); return; }
				// type, name, symbol, x, y (ignored: TODO later)
				auto type = pe.as<std::string>(args[0]);
				auto name = pe.as<std::string>(args[1]);
				auto symbol = pe.as<std::string>(args[2]);
				auto x = pe.as<double>(args[3]);
				auto y = pe.as<double>(args[4]);
				// check for argument errors
				if (!pe.getErrorMessage().empty()){ return; }
				if (!symbol.size()) { error("Symbol cannot be empty!"); return; }
				
				AgentBase* agent;
				if (type == "Player"){
					agent = &world.AddAgent<cse491::TrashInterface>(name, "symbol", symbol[0]);
					agent->SetPosition(x, y);
				} else {
					error("Unknown agent type!"); return;
				}
				
				pe.pushStack(static_cast<double>(agent->GetID()));
			});
			// Set agent property
			registerFunction("setAgentProperty", [this, &world](ProgramExecutor& pe){
				auto args = pe.popArgs();
				if (args.size() != 3) { error("Wrong number of arguments!"); return; }
				auto agent_id = static_cast<size_t>(pe.as<double>(args[0]));
				auto prop = pe.as<std::string>(args[1]);
				auto value = args[2];
				
				// check for argument errors
				if (!pe.getErrorMessage().empty()){ return; }
				
				AgentBase& agent = world.GetAgent(agent_id);
				if (pe.has<double>(value)){
					agent.SetProperty(prop, as<double>(value));
				} else if (pe.has<std::string>(value)){
					agent.SetProperty(prop, as<std::string>(value));
				} else {
					error("Unsupported property type!");
				}
//				pe.pushStack(static_cast<double>(agent->GetID()));
			});
			// Get agent property
			registerFunction("getAgentProperty", [this, &world](ProgramExecutor& pe){
				auto args = pe.popArgs();
				if (args.size() != 2) { error("Wrong number of arguments!"); return; }
				auto agent_id = static_cast<size_t>(pe.as<double>(args[0]));
				auto prop = pe.as<std::string>(args[1]);
				
				// check for argument errors
				if (!pe.getErrorMessage().empty()){ return; }
				
				AgentBase& agent = world.GetAgent(agent_id);
				if(agent.HasProperty(prop)){
					// property exists but type is unknown
					pushStack(agent.GetProperty<double>(prop));
				}
				error("Unsupported property type!");
//				pe.pushStack(static_cast<double>(agent->GetID()));
			});
			// Get agent position
			registerFunction("getAgentPosition", [this, &world](ProgramExecutor& pe){
				auto args = pe.popArgs();
				if (args.size() != 1) { error("Wrong number of arguments!"); return; }
				auto agent_id = static_cast<size_t>(pe.as<double>(args[0]));
				
				// check for argument errors
				if (!pe.getErrorMessage().empty()){ return; }
				
				AgentBase& agent = world.GetAgent(agent_id);
				auto x = agent.GetPosition().GetX();
				auto y = agent.GetPosition().GetY();
				pushStack(x);
				pushStack(y);
//				pe.pushStack(static_cast<double>(agent->GetID()));
			});
			// Set agent position
			registerFunction("setAgentPosition", [this, &world](ProgramExecutor& pe){
				auto args = pe.popArgs();
				if (args.size() != 3) { error("Wrong number of arguments!"); return; }
				auto agent_id = static_cast<size_t>(pe.as<double>(args[0]));
				auto x = pe.as<double>(args[1]);
				auto y = pe.as<double>(args[2]);
				// check for argument errors
				if (!pe.getErrorMessage().empty()){ return; }
				
				AgentBase& agent = world.GetAgent(agent_id);
				agent.SetPosition(x, y);
//				pe.pushStack(static_cast<double>(agent->GetID()));
			});
		}
		
		virtual ~ProgramExecutor() = default;
		
		/// Moves the interpreter's program counter to the end of the current block.
		/// 
		/// @param nest Optional starting nest value (defaults to zero)
		/// Begins as if it was nested within this many `start_block` operations.
		void skipBlock(int nest = 0){
			do {
				auto& unit = code->at(++index);
				if (unit.type == Unit::Type::operation && unit.value == "start_block") nest++;
				if (unit.type == Unit::Type::operation && unit.value == "end_block") nest--;
			} while(nest);
			// points to one past end_block
			index--;
			// points to end_block
		}
		
		/// @brief Registers a function on this ProgramExecutor object.
		/// 
		/// This allows the given function to be called from the interpreter via
		/// a function called `name`.
		/// 
		/// @note Names can be overridden by the user's program if they overwrite @p name.
		/// 
		/// @param name Function name
		/// @param callable Function accepting a ProgramExecutor& with no return.
		void registerFunction(std::string name, Callable callable){
			variables.insert_or_assign(name, callable);
		}
		
		/// @brief Retrieves a single value from the interpreter value stack.
		///
		/// @return Value object from stack.
		Value popStack(){
			auto v = stack.top();
			stack.pop();
			return v;
		}
		
		/// @brief Returns all arguments passed to an interpreter function.
		///
		/// This function retrieves the arguments of the function in the same
		/// order as in the source code. The end of the argument list is determined
		/// by an internal special Identifier 
		/// 
		/// This function should be called once for any Callable to get the arguments passed.
		/// 
		/// @return Vector of Values provided to an interpreter function.
		std::vector<Value> popArgs(){
			std::vector< Value > values;
			do {
				values.push_back(popStack());
			} while (!(has<Identifier>(values.back()) 
					&& static_cast<std::string>(as<Identifier>(values.back())) == "__INTERNAL_ENDARGS"));
			values.pop_back(); // don't keep that one
			std::ranges::reverse(values);
			return values;
		}
		
		/// @brief Pushes a single Value onto the interpreter value stack.
		/// 
		/// @param value Value to push to interpreter stack
		void pushStack(Value value){
			stack.push(value);
		}
		
		/// @brief Check whether or not this Value contains the expected type.
		/// 
		/// Checks the type of the given Value. If the type does not match and
		/// cannot be obtained, sets the interpreter error message and returns
		/// false. Otherwise, returns true.
		/// 
		/// @param a Value to validate type of.
		/// @return true if type is usable
		template <typename T>
		bool has(const Value& a){
			if (std::holds_alternative<T>(a)){
				return true;
			} else if (std::holds_alternative<Identifier>(a)){
				try {
					auto val = variables.at(static_cast<std::string>(std::get<Identifier>(a)));
					return std::holds_alternative<T>(val);
				} catch(const std::out_of_range& e) {
					error("Variable does not exist!");
				}
			}
			return false;
		}
		
		/// @brief Get a value of type T from provided Value
		/// 
		/// Gets the value of type T from given Value whether it contains a
		/// value or an identifier storing that value.
		/// 
		/// If the value cannot be accessed, sets the error message and returns
		/// a default-constructed value.
		/// 
		/// For example, if your program consists of `a=5` and `b=a`
		/// then as<double> will handle both 5 and a correctly as arguments
		/// std::get<double> is longer and only handles 5.
		/// 
		/// @param a Value to retrieve value from
		/// @return Value of type T
		template <typename T>
		T as(const Value& a){
			if (std::holds_alternative<T>(a)){
				return std::get<T>(a);
			} else if (std::holds_alternative<Identifier>(a)){
				auto val = variables.at(static_cast<std::string>(std::get<Identifier>(a)));
				if (std::holds_alternative<T>(val)){
					return std::get<T>(val);
				}
			}
			// error if conversion fails
			error(std::string{"Type error in as()! Expected "}+typeid(T).name());
			return T{};
		}
		
		/// @brief Gets the value of a variable as type T.
		/// 
		/// @throw std::out_of_range if it is not defined
		/// @throw std::bad_variant_access if variable is wrong type
		/// @param name Variable name to check
		/// @return Value of variable as type T
		template <typename T>
		T var(const std::string& name){
			auto val = variables.at(name);
			return std::get<T>(val);
		}
		
		void setVariable(const std::string& name, Value value){
			variables.insert_or_assign(name, value);
		}
		
		/// @brief Sets the error message and end interpreter execution.
		/// 
		/// Sets the stored error message for the interpreter. Only the first
		/// error set is saved.
		/// 
		/// @param error Message to store
		void error(const std::string& error){
			if (error_message.empty()){
				error_message = error;
			}
		}
		
		/// @brief Gets the error message stored.
		/// 
		/// Gets the error message from the interpreter. If no error was set,
		/// this will be the empty string.
		///
		/// @return Error message
		std::string getErrorMessage(){
			return error_message;
		}
		
		/// @brief Executes a program from a file.
		/// 
		/// @param filename File to load
		/// @return true if program ran successfully, false if an error occured
		bool runFile(const std::string& filename){
			//TODO: program preprocessing (add newline to end, remove spaces)
			if (!scripts.count(filename)){
				std::ifstream in{filename};
				std::string s;
				std::string filedata;
				while (getline(in, s))
					filedata += s + '\n';
				
				scripts[filename] = parse_to_code(filedata);
				if (scripts[filename].empty()){
					// implies a parse error
					error("Error parsing program from file");
					return false;
				}
			}
			code = &scripts[filename];
			return run();
		}
		
		/// @brief Executes a program from a string.
		/// 
		/// Executes a program from a string. This is the main interpreter function.
		/// See Language.hpp for most interesting syntax and parsing details.
		/// 
		/// @param program Program to run.
		/// @return true if program ran successfully, false if an error occured
		bool run(const std::string& program){
			scripts["__STRING_PROGRAM"] = parse_to_code(program);
			code = &scripts["__STRING_PROGRAM"];
			if (code->empty()){
				error("Error parsing program from string");
				return false;
			}
			return run();
		}
		
		bool run(){
			auto log = Logger::Log();
			log << Team::TEAM_4 << LogLevel::INFO << "Entering program execution" << std::endl;
			
			error_message = "";
			
			log << LogLevel::DEBUG;
			
			index = 0;
			while (error_message.empty() && index < code->size()){
				auto& unit = code->at(index);
				switch (unit.type){
					case Unit::Type::number:
						log << "Push number " << unit.value << std::endl;
						try {
							pushStack(std::stod(unit.value));
						} catch (const std::invalid_argument& e) {
							error("Failed to convert number!");
						} catch (const std::out_of_range& e){
							error("Number too big!");
						}
						break;
					
					case Unit::Type::string:
						log << "Push string " << unit.value << std::endl;
						pushStack(unit.value);
						break;
					
					case Unit::Type::identifier:
						log << "Push identifier " << unit.value << std::endl;
						pushStack(Identifier{unit.value});
						break;
					
					case Unit::Type::operation:
						// perform operation!
						log << "Perform operation " << unit.value << std::endl;
						if (unit.value == "="){
							// values to assign
							std::vector< Value > values = popArgs();
							// identifiers to assign to
							auto identifier_values = popArgs();
							if (values.size() > identifier_values.size()){
								error("Too many values!");
								break;
							} else if (values.size() < identifier_values.size()){
								error("Not enough values!");
								break;
							}
							// Convert to identifiers specifically
							std::vector< Identifier > identifiers;
							std::ranges::transform(identifier_values, std::back_inserter(identifiers), [this](const Value& v){ return as<Identifier>(v); });
							//ex. a,b,c=1,2,3 becomes the follwoing units
							// . a b c . 1 2 3 =
							// v: 3 2 1
							// i: c b a
							if (!getErrorMessage().empty()){
								// transform failed in some way 
								break;
							}
							
							// upon reaching this point, values and identifiers
							// are the same length and contain valid items.
							for (size_t i = 0; i < identifiers.size(); ++i){
								auto a = identifiers[i];
								auto b = values[i];
								
								if (!std::holds_alternative<Identifier>(b)){
									variables.insert_or_assign(static_cast<std::string>(a), b);
								} else {
									// exception if variable b does not exist
									try {
										auto& b_var = variables.at(static_cast<std::string>(as<Identifier>(b)));
										variables.insert_or_assign(static_cast<std::string>(a), b_var);
									} catch (const std::out_of_range& e){
										error("Variable did not exist!");
									}
								}
							}
						} else if (std::string{"+ - * / == != <= >= < >"}.find(unit.value) != std::string::npos){
							// binary expressions
							auto b = popStack();
							auto a = popStack();
							if (unit.value == "+"){
								if (has<double>(a) && has<double>(b)){
									pushStack(as<double>(a) + as<double>(b));
								} else if (has<std::string>(a) && has<std::string>(b)){
									pushStack(as<std::string>(a) + as<std::string>(b));
								} else {
									error("Runtime type error (plus)");
								}
							} else if (unit.value == "-"){
								if (has<double>(a) && has<double>(b)){
									pushStack(as<double>(a) - as<double>(b));
								} else {
									error("Runtime type error (minus)");
								}
							} else if (unit.value == "*"){
								if (has<double>(a) && has<double>(b)){
									pushStack(as<double>(a) * as<double>(b));
								} else if (has<std::string>(a) && has<double>(b)){
									std::string n;
									double c = as<double>(b);
									for (int i = 0; i < c; ++i){
										n += as<std::string>(a);
									}
									pushStack(n);
								} else {
									error("Runtime type error (times)");
								}
							} else if (unit.value == "/"){
								if (has<double>(a) && has<double>(b)){
									pushStack(as<double>(a) / as<double>(b));
								} else {
									error("Runtime type error (divide)");
								}
							} else if (unit.value == "=="){
								if (has<double>(a) && has<double>(b)){
									pushStack(static_cast<double>(as<double>(a) == as<double>(b)));
								} else if (has<std::string>(a) && has<std::string>(b)){
									pushStack(static_cast<double>(as<std::string>(a) == as<std::string>(b)));
								} else {
									error("Runtime type error (==)");
								}
							} else if (unit.value == "!="){
								if (has<double>(a) && has<double>(b)){
									pushStack(static_cast<double>(as<double>(a) != as<double>(b)));
								} else if (has<std::string>(a) && has<std::string>(b)){
									pushStack(static_cast<double>(as<std::string>(a) != as<std::string>(b)));
								} else {
									error("Runtime type error (!=)");
								}
							} else if (unit.value == "<"){
								if (has<double>(a) && has<double>(b)){
									pushStack(static_cast<double>(as<double>(a) < as<double>(b)));
								} else if (has<std::string>(a) && has<std::string>(b)){
									pushStack(static_cast<double>(as<std::string>(a) < as<std::string>(b)));
								} else {
									error("Runtime type error (<)");
								}
							} else if (unit.value == ">"){
								if (has<double>(a) && has<double>(b)){
									pushStack(static_cast<double>(as<double>(a) > as<double>(b)));
								} else if (has<std::string>(a) && has<std::string>(b)){
									pushStack(static_cast<double>(as<std::string>(a) > as<std::string>(b)));
								} else {
									error("Runtime type error (>)");
								}
							} else if (unit.value == "<="){
								if (has<double>(a) && has<double>(b)){
									pushStack(static_cast<double>(as<double>(a) <= as<double>(b)));
								} else if (has<std::string>(a) && has<std::string>(b)){
									pushStack(static_cast<double>(as<std::string>(a) <= as<std::string>(b)));
								} else {
									error("Runtime type error (<=)");
								}
							} else if (unit.value == ">="){
								if (has<double>(a) && has<double>(b)){
									pushStack(static_cast<double>(as<double>(a) >= as<double>(b)));
								} else if (has<std::string>(a) && has<std::string>(b)){
									pushStack(static_cast<double>(as<std::string>(a) >= as<std::string>(b)));
								} else {
									error("Runtime type error (>=)");
								}
							}
						} else if (unit.value == "endline"){
							// clear stack on end of line
							while (stack.size())
								popStack();
						} else if (unit.value == "endargs"){
							// this could absolutely be broken but that's OK
							pushStack(Identifier{"__INTERNAL_ENDARGS"});
						} else if (unit.value == "start_block"){
							auto type = as<std::string>(call_stack.top().at(0));
							if (type == "__FOR_BLOCK"){
								auto info = call_stack.top();
								
								double value = as<double>(info.at(1));
								double start = as<double>(info.at(2));
								double end = as<double>(info.at(3));
								double step = as<double>(info.at(4));
								
								if ((start < end && step > 0 && value + step <= end)
									|| (start >= end && step < 0 && value + step >= end)){
									// continue
									variables.insert_or_assign(static_cast<std::string>(as<Identifier>(info.at(1))), value + step);
									// jump back to the beginning to check the condiiton
//									index = static_cast<int>(as<double>(call_stack.top().at(5)));
								} else {
									// end loop
									call_stack.pop();
									skipBlock(1);
									index++; // skip end_block or else it tries to jump to beginning
									break;
								}
							}
						} else if (unit.value == "end_block"){
							// check for for loop if needed 
							auto type = as<std::string>(call_stack.top().at(0));
							if (type == "__FOR_BLOCK"){
								// jump back to the beginning to check the condiiton
								index = static_cast<int>(as<double>(call_stack.top().at(5)));
							} else if (type == "__IF_BLOCK"){
								// this only runs once, don't really need to save this
								call_stack.pop();
							}
						} else {
							error("Unknown operation '" + unit.value + "'");
						}
						break;
					
					case Unit::Type::function:
						log << "Perform function " << unit.value << std::endl;
						if (variables.count(unit.value)){
							auto& func = variables.at(unit.value);
							if (std::holds_alternative<Callable>(func)){
								std::get<Callable>(func)(*this);
							} else {
								error(unit.value + " is not a callable object!");
							}
							break;
						} else {
							error("Function " + unit.value + " does not exist!");
						}
						break;
					
					default:
						error("Unknown code unit '" + unit.value +"'!");
				}
				index++;
			}
			
			log << "Program execution ends" << std::endl;
			if (!error_message.empty()){
				log << LogLevel::WARNING << "With error: " << error_message << std::endl;
			}
			
			return error_message.empty();
		}
	};
} //worldlang
