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
#pragma once

#include <libsolidity/inlineasm/AsmDataForward.h>
#include <libsolidity/inlineasm/AsmAnalysisInfo.h>

#include <libsolidity/interface/Exceptions.h>

#include <boost/variant.hpp>
#include <boost/optional.hpp>

#include <set>

namespace dev
{
namespace julia
{

/**
 * Optimiser component that modifies an AST in place, inlining functions that can be
 * inlined inside functional expressions, i.e. functions that
 *  - return a single value
 *  - have a body like r := <functional expression>
 *  - neither reference themselves nor r in the right hand side
 *
 * Furthermore, the arguments of the function call cannot have any side-effects.
 *
 * This component can only be used on sources with unique names.
 */
class FunctionalInliner: public boost::static_visitor<bool>
{
public:
	using Statement = solidity::assembly::Statement;

	FunctionalInliner(solidity::assembly::Block& _block):
		m_block(_block)
	{}

	void run();

	bool operator()(solidity::assembly::Literal&) { return true; }
	bool operator()(solidity::assembly::Instruction&) { solAssert(false, ""); return false; }
	bool operator()(solidity::assembly::Identifier&) { return true; }
	bool operator()(solidity::assembly::FunctionalInstruction& _instr);
	bool operator()(solidity::assembly::FunctionCall&);
	bool operator()(solidity::assembly::Label&) { solAssert(false, ""); }
	bool operator()(solidity::assembly::StackAssignment&) { solAssert(false, ""); }
	bool operator()(solidity::assembly::Assignment& _assignment);
	bool operator()(solidity::assembly::VariableDeclaration& _varDecl);
	bool operator()(solidity::assembly::If& _if);
	bool operator()(solidity::assembly::Switch& _switch);
	bool operator()(solidity::assembly::FunctionDefinition&);
	bool operator()(solidity::assembly::ForLoop&);
	bool operator()(solidity::assembly::Block& _block);

private:
	bool tryInline(Statement& _statement);
	Statement replace(Statement const& _statement, std::map<std::string, Statement const*> const& _replacements);

	std::map<std::string, solidity::assembly::FunctionDefinition const*> m_inlinableFunctions;
	std::map<std::string, std::string> m_varReplacements;

	solidity::assembly::Block& m_block;
};


}
}
