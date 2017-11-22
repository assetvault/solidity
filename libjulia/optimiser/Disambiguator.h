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
 * Optimiser component that makes all identifiers unique.
 */

#pragma once

#include <libsolidity/inlineasm/AsmDataForward.h>
#include <libsolidity/inlineasm/AsmAnalysisInfo.h>

#include <boost/variant.hpp>
#include <boost/optional.hpp>

#include <set>

namespace dev
{
namespace julia
{

/**
 * Creates a copy of a iulia AST replacing all identifiers by unique names.
 */
class Disambiguator: public boost::static_visitor<solidity::assembly::Statement>
{
	using Statement = solidity::assembly::Statement;
public:
	Disambiguator(
		solidity::assembly::Block const& _block,
		solidity::assembly::AsmAnalysisInfo const& _analysisInfo
	):
		m_block(_block), m_info(_analysisInfo)
	{}

	std::shared_ptr<solidity::assembly::Block> run();

public:
	Statement operator()(solidity::assembly::Literal const& _literal);
	Statement operator()(solidity::assembly::Instruction const& _instruction);
	Statement operator()(solidity::assembly::Identifier const& _identifier);
	Statement operator()(solidity::assembly::FunctionalInstruction const& _instr);
	Statement operator()(solidity::assembly::FunctionCall const&);
	Statement operator()(solidity::assembly::Label const& _label);
	Statement operator()(solidity::assembly::StackAssignment const& _assignment);
	Statement operator()(solidity::assembly::Assignment const& _assignment);
	Statement operator()(solidity::assembly::VariableDeclaration const& _varDecl);
	Statement operator()(solidity::assembly::If const& _if);
	Statement operator()(solidity::assembly::Switch const& _switch);
	Statement operator()(solidity::assembly::FunctionDefinition const&);
	Statement operator()(solidity::assembly::ForLoop const&);
	Statement operator()(solidity::assembly::Block const& _block);

private:
	class TemporaryScope;

	template <typename T>
	std::vector<T> translateVector(std::vector<T> const& _values);

	template <typename T>
	std::shared_ptr<T> translate(std::shared_ptr<T> const& _v) { return _v ? std::make_shared<T>(translate(*_v)) : nullptr; }
	Statement translate(Statement const& _statement);
	solidity::assembly::Block translate(solidity::assembly::Block const& _block);
	solidity::assembly::Case translate(solidity::assembly::Case const& _case);
	solidity::assembly::Identifier translate(solidity::assembly::Identifier const& _identifier);
	solidity::assembly::Literal translate(solidity::assembly::Literal const& _literal);
	solidity::assembly::TypedName translate(solidity::assembly::TypedName const& _typedName);
	std::string translateIdentifier(std::string const& _name);

	solidity::assembly::Block const& m_block;
	solidity::assembly::AsmAnalysisInfo const& m_info;

	solidity::assembly::Scope* m_scope = nullptr;
	std::map<void const*, std::string> m_translations;
	std::set<std::string> m_usedNames;
};

template <typename T>
std::vector<T> Disambiguator::translateVector(std::vector<T> const& _values)
{
	std::vector<T> translated;
	for (auto const& v: _values)
		translated.emplace_back(translate(v));
	return translated;
}


}
}
