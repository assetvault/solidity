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

#include <libjulia/optimiser/Disambiguator.h>

#include <libsolidity/inlineasm/AsmData.h>
#include <libsolidity/inlineasm/AsmScope.h>

#include <libsolidity/interface/Exceptions.h>

using namespace std;
using namespace dev;
using namespace dev::julia;
using namespace dev::solidity;
using namespace dev::solidity::assembly;

class Disambiguator::TemporaryScope
{
public:
	TemporaryScope(Disambiguator& _disambiguator, Scope* _newScope):
		m_disambiguator(_disambiguator),
		m_originalScope(_disambiguator.m_scope)
	{
		m_disambiguator.m_scope = _newScope;
	}
	~TemporaryScope()
	{
		m_disambiguator.m_scope = m_originalScope;
	}

private:
	Disambiguator& m_disambiguator;
	Scope* m_originalScope = nullptr;

};

shared_ptr<Block> Disambiguator::run()
{
	return make_shared<Block>(translate(m_block));
}

solidity::assembly::Statement Disambiguator::operator ()(assembly::Instruction const& _instruction)
{
	return _instruction;
}

solidity::assembly::Statement Disambiguator::operator()(VariableDeclaration const& _varDecl)
{
	return VariableDeclaration{
		_varDecl.location,
		translateVector(_varDecl.variables),
		translate(_varDecl.value)
	};
}

solidity::assembly::Statement Disambiguator::operator()(Assignment const& _assignment)
{
	return Assignment{
		_assignment.location,
		translateVector(_assignment.variableNames),
		translate(_assignment.value)
	};
}

solidity::assembly::Statement Disambiguator::operator()(StackAssignment const&)
{
	solAssert(false, "Invalid operation.");
	return {};
}

solidity::assembly::Statement Disambiguator::operator()(Label const&)
{
	solAssert(false, "Invalid operation.");
	return {};
}

solidity::assembly::Statement Disambiguator::operator()(FunctionCall const& _call)
{
	return FunctionCall{
		_call.location,
		translate(_call.functionName),
		translateVector(_call.arguments)
	};
}

solidity::assembly::Statement Disambiguator::operator()(FunctionalInstruction const& _instruction)
{
	return FunctionalInstruction{
		_instruction.location,
		_instruction.instruction,
		translateVector(_instruction.arguments)
	};
}

solidity::assembly::Statement Disambiguator::operator()(assembly::Identifier const& _identifier)
{
	return Identifier{_identifier.location, translateIdentifier(_identifier.name)};
}

solidity::assembly::Statement Disambiguator::operator()(assembly::Literal const& _literal)
{
	return translate(_literal);
}

solidity::assembly::Statement Disambiguator::operator()(If const& _if)
{
	return If{_if.location, translate(_if.condition), translate(_if.body)};
}

solidity::assembly::Statement Disambiguator::operator()(Switch const& _switch)
{
	return Switch{_switch.location, translate(_switch.expression), translateVector(_switch.cases)};
}

solidity::assembly::Statement Disambiguator::operator()(FunctionDefinition const& _function)
{
	string translatedName = translateIdentifier(_function.name);

	solAssert(m_scope, "");
	solAssert(m_scope->identifiers.count(_function.name), "");

	TemporaryScope s(*this, m_info.scopes.at(m_info.virtualBlocks.at(&_function).get()).get());

	return FunctionDefinition{
		_function.location,
		move(translatedName),
		translateVector(_function.arguments),
		translateVector(_function.returns),
		translate(_function.body)
	};
}

solidity::assembly::Statement Disambiguator::operator()(ForLoop const& _forLoop)
{
	TemporaryScope s(*this, m_info.scopes.at(&_forLoop.pre).get());

	return ForLoop{
		_forLoop.location,
		translate(_forLoop.pre),
		translate(_forLoop.condition),
		translate(_forLoop.post),
		translate(_forLoop.body)
	};
}

solidity::assembly::Statement Disambiguator::operator ()(Block const& _block)
{
	return translate(_block);
}

Disambiguator::Statement Disambiguator::translate(Disambiguator::Statement const& _statement)
{
	return boost::apply_visitor(*this, _statement);
}

Block Disambiguator::translate(Block const& _block)
{
	TemporaryScope s(*this, m_info.scopes.at(&_block).get());

	return Block{_block.location, translateVector(_block.statements)};
}

Case Disambiguator::translate(Case const& _case)
{
	return Case{_case.location, translate(_case.value), translate(_case.body)};
}

Identifier Disambiguator::translate(Identifier const& _identifier)
{
	return Identifier{_identifier.location, translateIdentifier(_identifier.name)};
}

Literal Disambiguator::translate(Literal const& _literal)
{
	return _literal;
}

TypedName Disambiguator::translate(TypedName const& _typedName)
{
	return TypedName{_typedName.location, translateIdentifier(_typedName.name), _typedName.type};
}

string Disambiguator::translateIdentifier(string const& _originalName)
{
	solAssert(m_scope, "");
	Scope::Identifier const* id = m_scope->lookup(_originalName);
	solAssert(id, "");
	if (!m_translations.count(id))
	{
		string translated = _originalName;
		size_t suffix = 0;
		while (m_usedNames.count(translated))
		{
			suffix++;
			translated = _originalName + "_" + std::to_string(suffix);
		}
		m_usedNames.insert(translated);
		m_translations[id] = translated;
	}
	return m_translations.at(id);

}
