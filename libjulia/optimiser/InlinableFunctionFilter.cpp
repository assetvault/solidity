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
 * Optimiser component that identifies functions to be inlined.
 */

#include <libjulia/optimiser/InlinableFunctionFilter.h>

#include <libsolidity/inlineasm/AsmData.h>

#include <libsolidity/interface/Exceptions.h>

using namespace std;
using namespace dev;
using namespace dev::julia;
using namespace dev::solidity;
using namespace dev::solidity::assembly;


bool InlinableFunctionFilter::operator()(Identifier const& _identifier)
{
	return allowed(_identifier.name);
}

bool InlinableFunctionFilter::operator()(FunctionalInstruction const& _instr)
{
	return all(_instr.arguments);
}

bool InlinableFunctionFilter::operator()(FunctionCall const& _funCall)
{
	return allowed(_funCall.functionName.name) && all(_funCall.arguments);
}

bool InlinableFunctionFilter::operator()(If const& _if)
{
	(*this)(_if.body);
	return false;
}

bool InlinableFunctionFilter::operator()(Switch const& _switch)
{
	for (auto const& _case: _switch.cases)
		(*this)(_case.body);
	return false;
}

bool InlinableFunctionFilter::operator()(FunctionDefinition const& _function)
{
	if (_function.returns.size() == 1 && _function.body.statements.size() == 1)
	{
		string const& retVariable = _function.returns.front().name;
		Statement const& bodyStatement = _function.body.statements.front();
		if (bodyStatement.type() == typeid(assembly::Assignment))
		{
			assembly::Assignment const& assignment = boost::get<Assignment>(bodyStatement);
			if (assignment.variableNames.size() == 1 && assignment.variableNames.front().name == retVariable)
			{
				// We can just set it here, because another function definition inside this
				// function will anyway make this non-inlinable.
				m_disallowedIdentifiers = set<string>{retVariable, _function.name};
				if (boost::apply_visitor(*this, *assignment.value))
					m_inlinableFunctions[_function.name] = &_function;
				return false;
			}
		}
	}
	(*this)(_function.body);
	return false;
}

bool InlinableFunctionFilter::operator()(ForLoop const& _loop)
{
	(*this)(_loop.pre);
	(*this)(_loop.post);
	(*this)(_loop.body);
	return false;
}

bool InlinableFunctionFilter::operator()(Block const& _block)
{
	all(_block.statements);
	return false;
}

bool InlinableFunctionFilter::all(std::vector<Statement> const& _statements)
{
	bool failed = false;
	for (auto const& st: _statements)
		if (!boost::apply_visitor(*this, st))
			failed = true;
	return !failed;
}
