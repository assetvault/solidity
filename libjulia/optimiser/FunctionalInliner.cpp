/*
	This file is part of solidity.

	solidity is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	solidity is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with solidity.  If not, see <http://www.gnu.org/licenses/>.
*/
/**
 * Optimiser component that performs function inlining.
 */

#include <libjulia/optimiser/FunctionalInliner.h>

#include <libjulia/optimiser/InlinableFunctionFilter.h>

#include <libsolidity/inlineasm/AsmData.h>
#include <libsolidity/inlineasm/AsmScope.h>

#include <libevmasm/SemanticInformation.h>

#include <libsolidity/interface/Exceptions.h>

using namespace std;
using namespace dev;
using namespace dev::julia;
using namespace dev::solidity;
using namespace dev::solidity::assembly;

void FunctionalInliner::run()
{
	InlinableFunctionFilter filter;
	filter(m_block);
	m_inlinableFunctions = filter.inlinableFunctions();

	(*this)(m_block);
}

bool FunctionalInliner::operator()(FunctionalInstruction& _instr)
{
	bool pure = eth::SemanticInformation::movable(_instr.instruction.instruction);

	for (auto& arg: _instr.arguments)
		if (!tryInline(arg))
			pure = false;
	return pure;
}

bool FunctionalInliner::operator()(FunctionCall&)
{
	solAssert(false, "Should have called tryInline instead.");
	return false;
}

bool FunctionalInliner::operator()(Assignment& _assignment)
{
	solAssert(_assignment.value, "");
	tryInline(*_assignment.value);
	return false;
}

bool FunctionalInliner::operator()(VariableDeclaration& _varDecl)
{
	tryInline(*_varDecl.value);
	return false;
}

bool FunctionalInliner::operator()(If& _if)
{
	tryInline(*_if.condition);
	(*this)(_if.body);
	return false;
}

bool FunctionalInliner::operator()(Switch& _switch)
{
	tryInline(*_switch.expression);
	for (auto& cse: _switch.cases)
		(*this)(cse.body);
	return false;
}

bool FunctionalInliner::operator()(FunctionDefinition& _funDef)
{
	(*this)(_funDef.body);
	return false;
}

bool FunctionalInliner::operator()(ForLoop& _loop)
{
	(*this)(_loop.pre);
	tryInline(*_loop.condition);
	(*this)(_loop.post);
	(*this)(_loop.body);
	return false;
}

bool FunctionalInliner::operator()(Block& _block)
{
	for (auto& st: _block.statements)
		tryInline(st);
	return false;
}

bool FunctionalInliner::tryInline(Statement& _statement)
{
	if (_statement.type() == typeid(FunctionCall))
	{
		FunctionCall& funCall = boost::get<FunctionCall>(_statement);
		bool allArgsPure = true;

		for (auto& arg: funCall.arguments)
			if (!tryInline(arg))
				allArgsPure = false;
		// TODO: Could this have distorted the code so that a simple replacement does not
		// work anymore?

		if (allArgsPure && m_inlinableFunctions.count(funCall.functionName.name))
		{
			FunctionDefinition const& fun = *m_inlinableFunctions.at(funCall.functionName.name);
			map<string, Statement const*> replacements;
			for (size_t i = 0; i < fun.arguments.size(); ++i)
				replacements[fun.arguments[i].name] = &funCall.arguments[i];
			_statement = replace(*boost::get<Assignment>(fun.body.statements.front()).value, replacements);

			// TODO actually in the process of inlining, we could also make a funciton non-inlinable
			// because it could now call itself

			// Pureness of this depends on the pureness of the replacement,
			// i.e. the pureness of the function itself.
			// Perhaps we can just re-run?

			// If two functions call each other, we have to exit after some iterations.

			// Just return false for now.
			return false;
		}
		else
			return allArgsPure;
	}
	else
		return boost::apply_visitor(*this, _statement);
}

Statement FunctionalInliner::replace(Statement const& _statement, map<string, Statement const*> const& _replacements)
{
	if (_statement.type() == typeid(FunctionCall))
	{
		FunctionCall const& funCall = boost::get<FunctionCall>(_statement);
		vector<Statement> arguments;
		for (auto const& arg: funCall.arguments)
			arguments.push_back(replace(arg, _replacements));

		return FunctionCall{funCall.location, funCall.functionName, std::move(arguments)};
	}
	else if (_statement.type() == typeid(FunctionalInstruction))
	{
		FunctionalInstruction const& instr = boost::get<FunctionalInstruction>(_statement);
		vector<Statement> arguments;
		for (auto const& arg: instr.arguments)
			arguments.push_back(replace(arg, _replacements));

		return FunctionalInstruction{instr.location, instr.instruction, std::move(arguments)};
	}
	else if (_statement.type() == typeid(Identifier))
	{
		string const& name = boost::get<Identifier>(_statement).name;
		if (_replacements.count(name))
			return replace(*_replacements.at(name), {});
		else
			return _statement;
	}
	else if (_statement.type() == typeid(Literal))
		return _statement;

	solAssert(false, "");
	return {};
}
