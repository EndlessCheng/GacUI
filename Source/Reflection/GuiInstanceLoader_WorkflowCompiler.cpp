#include "GuiInstanceLoader_WorkflowCompiler.h"
#include "TypeDescriptors/GuiReflectionEvents.h"
#include "../Resources/GuiParserManager.h"

namespace vl
{
	namespace presentation
	{
		using namespace workflow;
		using namespace workflow::analyzer;
		using namespace workflow::runtime;
		using namespace reflection::description;
		using namespace collections;

		using namespace controls;
		using namespace compositions;

#define ERROR_CODE_PREFIX L"================================================================"

/***********************************************************************
Parser
***********************************************************************/

		Ptr<workflow::WfExpression> Workflow_ParseExpression(const WString& code, types::ErrorList& errors)
		{
			auto parser = GetParserManager()->GetParser<WfExpression>(L"WORKFLOW-EXPRESSION");
			return parser->TypedParse(code, errors);
		}

		Ptr<workflow::WfStatement> Workflow_ParseStatement(const WString& code, types::ErrorList& errors)
		{
			auto parser = GetParserManager()->GetParser<WfStatement>(L"WORKFLOW-STATEMENT");
			return parser->TypedParse(code, errors);
		}

/***********************************************************************
Workflow_InstallBindProperty
***********************************************************************/

		Ptr<workflow::WfStatement> Workflow_InstallUriProperty(GlobalStringKey variableName, description::IPropertyInfo* propertyInfo, const WString& protocol, const WString& path)
		{
			auto subBlock = MakePtr<WfBlockStatement>();
			{
				auto refResolver = MakePtr<WfReferenceExpression>();
				refResolver->name.value = L"<resolver>";

				auto member = MakePtr<WfMemberExpression>();
				member->parent = refResolver;
				member->name.value = L"ResolveResource";

				auto valueProtocol = MakePtr<WfStringExpression>();
				valueProtocol->value.value = protocol;

				auto valuePath = MakePtr<WfStringExpression>();
				valuePath->value.value = path;

				auto call = MakePtr<WfCallExpression>();
				call->function = member;
				call->arguments.Add(valueProtocol);
				call->arguments.Add(valuePath);

				auto varDesc = MakePtr<WfVariableDeclaration>();
				varDesc->name.value = L"<resource-item>";
				varDesc->expression = call;

				auto varStat = MakePtr<WfVariableStatement>();
				varStat->variable = varDesc;
				subBlock->statements.Add(varStat);
			}
			{
				auto refResourceItem = MakePtr<WfReferenceExpression>();
				refResourceItem->name.value = L"<resource-item>";

				auto isNull = MakePtr<WfTypeTestingExpression>();
				isNull->expression = refResourceItem;
				isNull->test = WfTypeTesting::IsNull;

				auto valueException = MakePtr<WfStringExpression>();
				valueException->value.value = L"Resource \"" + protocol + L"://" + path + L"\" does not exist.";

				auto raiseStat = MakePtr<WfRaiseExceptionStatement>();
				raiseStat->expression = valueException;
				
				auto ifBlock = MakePtr<WfBlockStatement>();
				ifBlock->statements.Add(raiseStat);

				auto ifStat = MakePtr<WfIfStatement>();
				ifStat->expression = isNull;
				ifStat->trueBranch = ifBlock;
				subBlock->statements.Add(ifStat);
			}

			auto td = propertyInfo->GetReturn()->GetTypeDescriptor();
			Ptr<ITypeInfo> convertedType;
			if (td->GetValueSerializer())
			{
				convertedType = TypeInfoRetriver<Ptr<GuiTextData>>::CreateTypeInfo();
			}
			else if (td == description::GetTypeDescriptor<INativeImage>() || td == description::GetTypeDescriptor<GuiImageData>())
			{
				convertedType = TypeInfoRetriver<Ptr<GuiImageData>>::CreateTypeInfo();
			}
			else
			{
				convertedType = CopyTypeInfo(propertyInfo->GetReturn());
			}

			{
				auto refResourceItem = MakePtr<WfReferenceExpression>();
				refResourceItem->name.value = L"<resource-item>";

				auto cast = MakePtr<WfTypeCastingExpression>();
				cast->expression = refResourceItem;
				cast->type = GetTypeFromTypeInfo(convertedType.Obj());
				cast->strategy = WfTypeCastingStrategy::Weak;

				auto varDesc = MakePtr<WfVariableDeclaration>();
				varDesc->name.value = L"<resource-value>";
				varDesc->expression = cast;

				auto varStat = MakePtr<WfVariableStatement>();
				varStat->variable = varDesc;
				subBlock->statements.Add(varStat);
			}
			{
				auto refResourceValue = MakePtr<WfReferenceExpression>();
				refResourceValue->name.value = L"<resource-value>";

				auto isNull = MakePtr<WfTypeTestingExpression>();
				isNull->expression = refResourceValue;
				isNull->test = WfTypeTesting::IsNull;

				auto valueException = MakePtr<WfStringExpression>();
				valueException->value.value = L"Resource \"" + protocol + L"://" + path + L"\" cannot be read as type \"" + convertedType->GetTypeDescriptor()->GetTypeName() + L"\".";

				auto raiseStat = MakePtr<WfRaiseExceptionStatement>();
				raiseStat->expression = valueException;

				auto ifBlock = MakePtr<WfBlockStatement>();
				ifBlock->statements.Add(raiseStat);

				auto ifStat = MakePtr<WfIfStatement>();
				ifStat->expression = isNull;
				ifStat->trueBranch = ifBlock;
				subBlock->statements.Add(ifStat);
			}

			Ptr<WfExpression> evalExpression;
			if (td->GetValueSerializer())
			{
				auto refResourceValue = MakePtr<WfReferenceExpression>();
				refResourceValue->name.value = L"<resource-value>";

				auto member = MakePtr<WfMemberExpression>();
				member->parent = refResourceValue;
				member->name.value = L"Text";

				auto cast = MakePtr<WfTypeCastingExpression>();
				cast->expression = member;
				cast->type = GetTypeFromTypeInfo(propertyInfo->GetReturn());
				cast->strategy = WfTypeCastingStrategy::Strong;

				evalExpression = cast;
			}
			else if (td == description::GetTypeDescriptor<INativeImage>())
			{
				auto refResourceValue = MakePtr<WfReferenceExpression>();
				refResourceValue->name.value = L"<resource-value>";

				auto member = MakePtr<WfMemberExpression>();
				member->parent = refResourceValue;
				member->name.value = L"Image";

				evalExpression = member;
			}
			else
			{
				auto refResourceValue = MakePtr<WfReferenceExpression>();
				refResourceValue->name.value = L"<resource-value>";

				evalExpression = refResourceValue;
			}

			{
				auto refSubscribee = MakePtr<WfReferenceExpression>();
				refSubscribee->name.value = variableName.ToString();

				auto member = MakePtr<WfMemberExpression>();
				member->parent = refSubscribee;
				member->name.value = propertyInfo->GetName();

				auto assign = MakePtr<WfBinaryExpression>();
				assign->op = WfBinaryOperator::Assign;
				assign->first = member;
				assign->second = evalExpression;

				auto stat = MakePtr<WfExpressionStatement>();
				stat->expression = assign;
				subBlock->statements.Add(stat);
			}
			return subBlock;
		}

/***********************************************************************
Workflow_InstallBindProperty
***********************************************************************/

		Ptr<workflow::WfStatement> Workflow_InstallBindProperty(GlobalStringKey variableName, description::IPropertyInfo* propertyInfo, Ptr<workflow::WfExpression> bindExpression)
		{
			auto subBlock = MakePtr<WfBlockStatement>();
			{
				auto refThis = MakePtr<WfReferenceExpression>();
				refThis->name.value = L"<this>";

				auto member = MakePtr<WfMemberExpression>();
				member->parent = refThis;
				member->name.value = L"AddSubscription";

				auto call = MakePtr<WfCallExpression>();
				call->function = member;
				call->arguments.Add(bindExpression);

				auto var = MakePtr<WfVariableDeclaration>();
				var->name.value = L"<subscription>";
				var->expression = call;

				auto stat = MakePtr<WfVariableStatement>();
				stat->variable = var;
				subBlock->statements.Add(stat);
			}
			{
				auto callback = MakePtr<WfFunctionDeclaration>();
				callback->anonymity = WfFunctionAnonymity::Anonymous;
				callback->returnType = GetTypeFromTypeInfo(TypeInfoRetriver<void>::CreateTypeInfo().Obj());;
				{
					auto arg = MakePtr<WfFunctionArgument>();
					arg->name.value = L"<value>";
					arg->type = GetTypeFromTypeInfo(TypeInfoRetriver<Value>::CreateTypeInfo().Obj());
					callback->arguments.Add(arg);
				}
				auto callbackBlock = MakePtr<WfBlockStatement>();
				callback->statement = callbackBlock;
				{
					auto refSubscribee = MakePtr<WfReferenceExpression>();
					refSubscribee->name.value = variableName.ToString();

					auto member = MakePtr<WfMemberExpression>();
					member->parent = refSubscribee;
					member->name.value = propertyInfo->GetName();

					auto var = MakePtr<WfVariableDeclaration>();
					var->name.value = L"<old>";
					var->expression = member;

					auto stat = MakePtr<WfVariableStatement>();
					stat->variable = var;
					callbackBlock->statements.Add(stat);
				}
				{
					ITypeInfo* propertyType = propertyInfo->GetReturn();
					if (propertyInfo->GetSetter() && propertyInfo->GetSetter()->GetParameterCount() == 1)
					{
						propertyType = propertyInfo->GetSetter()->GetParameter(0)->GetType();
					}

					auto refValue = MakePtr<WfReferenceExpression>();
					refValue->name.value = L"<value>";

					auto cast = MakePtr<WfTypeCastingExpression>();
					cast->strategy = WfTypeCastingStrategy::Strong;
					cast->expression = refValue;
					cast->type = GetTypeFromTypeInfo(propertyType);

					auto var = MakePtr<WfVariableDeclaration>();
					var->name.value = L"<new>";
					var->expression = cast;

					auto stat = MakePtr<WfVariableStatement>();
					stat->variable = var;
					callbackBlock->statements.Add(stat);
				}
				{
					auto refOld = MakePtr<WfReferenceExpression>();
					refOld->name.value = L"<old>";

					auto refNew = MakePtr<WfReferenceExpression>();
					refNew->name.value = L"<new>";

					auto compare = MakePtr<WfBinaryExpression>();
					compare->op = WfBinaryOperator::EQ;
					compare->first = refOld;
					compare->second = refNew;

					auto ifStat = MakePtr<WfIfStatement>();
					ifStat->expression = compare;
					callbackBlock->statements.Add(ifStat);

					auto ifBlock = MakePtr<WfBlockStatement>();
					ifStat->trueBranch = ifBlock;

					auto returnStat = MakePtr<WfReturnStatement>();
					ifBlock->statements.Add(returnStat);
				}
				{
					auto refSubscribee = MakePtr<WfReferenceExpression>();
					refSubscribee->name.value = variableName.ToString();

					auto member = MakePtr<WfMemberExpression>();
					member->parent = refSubscribee;
					member->name.value = propertyInfo->GetName();

					auto refNew = MakePtr<WfReferenceExpression>();
					refNew->name.value = L"<new>";

					auto assign = MakePtr<WfBinaryExpression>();
					assign->op = WfBinaryOperator::Assign;
					assign->first = member;
					assign->second = refNew;

					auto stat = MakePtr<WfExpressionStatement>();
					stat->expression = assign;
					callbackBlock->statements.Add(stat);
				}

				auto funcExpr = MakePtr<WfFunctionExpression>();
				funcExpr->function = callback;

				auto refThis = MakePtr<WfReferenceExpression>();
				refThis->name.value = L"<subscription>";

				auto member = MakePtr<WfMemberExpression>();
				member->parent = refThis;
				member->name.value = L"Subscribe";

				auto call = MakePtr<WfCallExpression>();
				call->function = member;
				call->arguments.Add(funcExpr);

				auto stat = MakePtr<WfExpressionStatement>();
				stat->expression = call;
				subBlock->statements.Add(stat);
			}
			{
				auto refThis = MakePtr<WfReferenceExpression>();
				refThis->name.value = L"<subscription>";

				auto member = MakePtr<WfMemberExpression>();
				member->parent = refThis;
				member->name.value = L"Update";

				auto call = MakePtr<WfCallExpression>();
				call->function = member;

				auto stat = MakePtr<WfExpressionStatement>();
				stat->expression = call;
				subBlock->statements.Add(stat);
			}

			return subBlock;
		}

/***********************************************************************
Workflow_InstallEvalProperty
***********************************************************************/

		Ptr<workflow::WfStatement> Workflow_InstallEvalProperty(GlobalStringKey variableName, description::IPropertyInfo* propertyInfo, Ptr<workflow::WfExpression> evalExpression)
		{
			auto refSubscribee = MakePtr<WfReferenceExpression>();
			refSubscribee->name.value = variableName.ToString();

			auto member = MakePtr<WfMemberExpression>();
			member->parent = refSubscribee;
			member->name.value = propertyInfo->GetName();

			auto assign = MakePtr<WfBinaryExpression>();
			assign->op = WfBinaryOperator::Assign;
			assign->first = member;
			assign->second = evalExpression;

			auto stat = MakePtr<WfExpressionStatement>();
			stat->expression = assign;
			
			return stat;
		}

/***********************************************************************
Workflow_InstallEvent
***********************************************************************/

		Ptr<workflow::WfStatement> Workflow_InstallEvent(GlobalStringKey variableName, description::IEventInfo* eventInfo, const WString& handlerName)
		{
			vint count = eventInfo->GetHandlerType()->GetElementType()->GetGenericArgumentCount() - 1;

			auto subBlock = MakePtr<WfBlockStatement>();
			{
				auto var = MakePtr<WfReferenceExpression>();
				var->name.value = variableName.ToString();

				auto member = MakePtr<WfMemberExpression>();
				member->parent = var;
				member->name.value = eventInfo->GetName();

				auto refThis = MakePtr<WfReferenceExpression>();
				refThis->name.value = L"<this>";

				auto handler = MakePtr<WfMemberExpression>();
				handler->parent = refThis;
				handler->name.value = handlerName;

				auto call = MakePtr<WfCallExpression>();
				call->function = handler;
				for (vint i = 0; i < count; i++)
				{
					auto argument = MakePtr<WfOrderedNameExpression>();
					argument->name.value = L"$" + itow(i + 1);
					call->arguments.Add(argument);
				}

				auto eventHandler = MakePtr<WfOrderedLambdaExpression>();
				eventHandler->body = call;

				auto attachEvent = MakePtr<WfAttachEventExpression>();
				attachEvent->event = member;
				attachEvent->function = eventHandler;

				auto stat = MakePtr<WfExpressionStatement>();
				stat->expression = attachEvent;
				subBlock->statements.Add(stat);
			}

			return subBlock;
		}

/***********************************************************************
Workflow_InstallEvalEvent
***********************************************************************/

		Ptr<workflow::WfStatement> Workflow_InstallEvalEvent(GlobalStringKey variableName, description::IEventInfo* eventInfo, Ptr<workflow::WfStatement> evalStatement)
		{
			auto func = MakePtr<WfFunctionDeclaration>();
			func->anonymity = WfFunctionAnonymity::Anonymous;
			func->returnType = GetTypeFromTypeInfo(TypeInfoRetriver<void>::CreateTypeInfo().Obj());

			vint count = eventInfo->GetHandlerType()->GetElementType()->GetGenericArgumentCount() - 1;
			bool standardName = false;
			if (count == 2)
			{
				auto senderType = eventInfo->GetHandlerType()->GetElementType()->GetGenericArgument(1)->GetTypeDescriptor();
				auto argumentType = eventInfo->GetHandlerType()->GetElementType()->GetGenericArgument(2)->GetTypeDescriptor();
				if (senderType == GetTypeDescriptor<GuiGraphicsComposition>())
				{
					auto expectedType = GetTypeDescriptor<GuiEventArgs>();
					List<ITypeDescriptor*> types;
					types.Add(argumentType);
					for (vint i = 0; i < types.Count(); i++)
					{
						auto type = types[i];
						if (type == expectedType)
						{
							standardName = true;
							break;
						}
						vint baseCount = type->GetBaseTypeDescriptorCount();
						for (vint j = 0; j < baseCount; j++)
						{
							auto baseType = type->GetBaseTypeDescriptor(j);
							if (!types.Contains(baseType))
							{
								types.Add(baseType);
							}
						}
					}
				}
			}

			if (standardName)
			{
				{
					auto arg = MakePtr<WfFunctionArgument>();
					arg->name.value = L"sender";
					arg->type = GetTypeFromTypeInfo(eventInfo->GetHandlerType()->GetElementType()->GetGenericArgument(1));
					func->arguments.Add(arg);
				}
				{
					auto arg = MakePtr<WfFunctionArgument>();
					arg->name.value = L"arguments";
					arg->type = GetTypeFromTypeInfo(eventInfo->GetHandlerType()->GetElementType()->GetGenericArgument(2));
					func->arguments.Add(arg);
				}
			}
			else
			{
				auto type = TypeInfoRetriver<Value>::CreateTypeInfo();
				for (vint i = 0; i < count; i++)
				{
					auto arg = MakePtr<WfFunctionArgument>();
					arg->name.value = L"<argument>" + itow(i + 1);
					arg->type = GetTypeFromTypeInfo(type.Obj());
					func->arguments.Add(arg);
				}
			}
						
			auto funcBlock = MakePtr<WfBlockStatement>();
			funcBlock->statements.Add(evalStatement);
			func->statement = funcBlock;

			auto subBlock = MakePtr<WfBlockStatement>();

			{
				auto eventHandlerLambda = MakePtr<WfFunctionExpression>();
				eventHandlerLambda->function = func;

				auto eventHandler = MakePtr<WfVariableDeclaration>();
				eventHandler->name.value = L"<event-handler>";
				eventHandler->expression = eventHandlerLambda;

				auto stat = MakePtr<WfVariableStatement>();
				stat->variable = eventHandler;
				subBlock->statements.Add(stat);
			}
			{
				auto var = MakePtr<WfReferenceExpression>();
				var->name.value = variableName.ToString();

				auto member = MakePtr<WfMemberExpression>();
				member->parent = var;
				member->name.value = eventInfo->GetName();

				auto eventHandler = MakePtr<WfReferenceExpression>();
				eventHandler->name.value = L"<event-handler>";

				auto attachEvent = MakePtr<WfAttachEventExpression>();
				attachEvent->event = member;
				attachEvent->function = eventHandler;

				auto stat = MakePtr<WfExpressionStatement>();
				stat->expression = attachEvent;
				subBlock->statements.Add(stat);
			}

			return subBlock;
		}

/***********************************************************************
Workflow_CreateEmptyModule
***********************************************************************/

		Ptr<workflow::WfModule> Workflow_CreateEmptyModule(Ptr<GuiInstanceContext> context)
		{
			auto module = MakePtr<WfModule>();
			vint index = context->namespaces.Keys().IndexOf(GlobalStringKey());
			if (index != -1)
			{
				auto nss = context->namespaces.Values()[index];
				FOREACH(Ptr<GuiInstanceNamespace>, ns, nss->namespaces)
				{
					auto path = MakePtr<WfModuleUsingPath>();
					module->paths.Add(path);

					auto pathCode = ns->prefix + L"*" + ns->postfix;
					auto reading = pathCode.Buffer();
					while (reading)
					{
						auto delimiter = wcsstr(reading, L"::");
						auto begin = reading;
						auto end = delimiter ? delimiter : begin + wcslen(reading);

						auto wildcard = wcschr(reading, L'*');
						if (wildcard >= end)
						{
							wildcard = nullptr;
						}

						auto item = MakePtr<WfModuleUsingItem>();
						path->items.Add(item);
						if (wildcard)
						{
							if (begin < wildcard)
							{
								auto fragment = MakePtr<WfModuleUsingNameFragment>();
								item->fragments.Add(fragment);
								fragment->name.value = WString(begin, vint(wildcard - begin));
							}
							{
								auto fragment = MakePtr<WfModuleUsingWildCardFragment>();
								item->fragments.Add(fragment);
							}
							if (wildcard + 1 < end)
							{
								auto fragment = MakePtr<WfModuleUsingNameFragment>();
								item->fragments.Add(fragment);
								fragment->name.value = WString(wildcard + 1, vint(end - wildcard - 1));
							}
						}
						else if (begin < end)
						{
							auto fragment = MakePtr<WfModuleUsingNameFragment>();
							item->fragments.Add(fragment);
							fragment->name.value = WString(begin, vint(end - begin));
						}

						if (delimiter)
						{
							reading = delimiter + 2;
						}
						else
						{
							reading = nullptr;
						}
					}
				}
			}
			return module;
		}
		
		Ptr<workflow::WfModule> Workflow_CreateModuleWithInitFunction(Ptr<GuiInstanceContext> context, types::VariableTypeInfoMap& typeInfos, description::ITypeDescriptor* rootTypeDescriptor, Ptr<workflow::WfStatement> functionBody)
		{
			auto module = Workflow_CreateEmptyModule(context);
			Workflow_CreateVariablesForReferenceValues(module, typeInfos);

			auto thisParam = MakePtr<WfFunctionArgument>();
			thisParam->name.value = L"<this>";
			{
				Ptr<TypeInfoImpl> elementType = new TypeInfoImpl(ITypeInfo::TypeDescriptor);
				elementType->SetTypeDescriptor(rootTypeDescriptor);

				Ptr<TypeInfoImpl> pointerType = new TypeInfoImpl(ITypeInfo::RawPtr);
				pointerType->SetElementType(elementType);

				thisParam->type = GetTypeFromTypeInfo(pointerType.Obj());
			}

			auto resolverParam = MakePtr<WfFunctionArgument>();
			resolverParam->name.value = L"<resolver>";
			{
				Ptr<TypeInfoImpl> elementType = new TypeInfoImpl(ITypeInfo::TypeDescriptor);
				elementType->SetTypeDescriptor(description::GetTypeDescriptor<GuiResourcePathResolver>());

				Ptr<TypeInfoImpl> pointerType = new TypeInfoImpl(ITypeInfo::RawPtr);
				pointerType->SetElementType(elementType);

				resolverParam->type = GetTypeFromTypeInfo(pointerType.Obj());
			}

			auto func = MakePtr<WfFunctionDeclaration>();
			func->anonymity = WfFunctionAnonymity::Named;
			func->name.value = L"<initialize-instance>";
			func->arguments.Add(thisParam);
			func->arguments.Add(resolverParam);
			func->returnType = GetTypeFromTypeInfo(TypeInfoRetriver<void>::CreateTypeInfo().Obj());
			func->statement = functionBody;
			module->declarations.Add(func);

			return module;
		}

/***********************************************************************
Variable
***********************************************************************/

		void Workflow_CreatePointerVariable(Ptr<workflow::WfModule> module, GlobalStringKey name, description::ITypeDescriptor* type)
		{
			auto var = MakePtr<WfVariableDeclaration>();
			var->name.value = name.ToString();
			{
				Ptr<TypeInfoImpl> elementType = new TypeInfoImpl(ITypeInfo::TypeDescriptor);
				elementType->SetTypeDescriptor(type);

				Ptr<TypeInfoImpl> pointerType = new TypeInfoImpl(ITypeInfo::RawPtr);
				pointerType->SetElementType(elementType);

				var->type = GetTypeFromTypeInfo(pointerType.Obj());
			}

			auto literal = MakePtr<WfLiteralExpression>();
			literal->value = WfLiteralValue::Null;
			var->expression = literal;

			module->declarations.Add(var);
		}
		
		void Workflow_CreateVariablesForReferenceValues(Ptr<workflow::WfModule> module, types::VariableTypeInfoMap& typeInfos)
		{
			for (vint i = 0; i < typeInfos.Count(); i++)
			{
				auto key = typeInfos.Keys()[i];
				auto value = typeInfos.Values()[i].typeDescriptor;
				Workflow_CreatePointerVariable(module, key, value);
			}
		}

		void Workflow_SetVariablesForReferenceValues(Ptr<workflow::runtime::WfRuntimeGlobalContext> context, Ptr<GuiInstanceEnvironment> env)
		{
			FOREACH_INDEXER(GlobalStringKey, name, index, env->scope->referenceValues.Keys())
			{
				vint variableIndex = context->assembly->variableNames.IndexOf(name.ToString());
				if (variableIndex != -1)
				{
					context->globalVariables->variables[variableIndex] = env->scope->referenceValues.Values()[index];
				}
			}
		}

/***********************************************************************
Workflow_ValidateStatement
***********************************************************************/

		bool Workflow_ValidateStatement(Ptr<GuiInstanceContext> context, types::VariableTypeInfoMap& typeInfos, description::ITypeDescriptor* rootTypeDescriptor, types::ErrorList& errors, const WString& code, Ptr<workflow::WfStatement> statement)
		{
			bool failed = false;
			auto module = Workflow_CreateModuleWithInitFunction(context, typeInfos, rootTypeDescriptor, statement);

			Workflow_GetSharedManager()->Clear(true, true);
			Workflow_GetSharedManager()->AddModule(module);
			Workflow_GetSharedManager()->Rebuild(true);
			if (Workflow_GetSharedManager()->errors.Count() > 0)
			{
				errors.Add(ERROR_CODE_PREFIX L"Failed to analyze the workflow code \"" + code + L"\".");
				FOREACH(Ptr<parsing::ParsingError>, error, Workflow_GetSharedManager()->errors)
				{
					errors.Add(error->errorMessage);
				}
				failed = true;
			}

			return !failed;
		}

/***********************************************************************
WorkflowReferenceNamesVisitor
***********************************************************************/

		class WorkflowReferenceNamesVisitor : public Object, public GuiValueRepr::IVisitor
		{
		public:
			Ptr<GuiInstanceContext>				context;
			types::VariableTypeInfoMap&			typeInfos;
			types::ErrorList&					errors;

			IGuiInstanceLoader::TypeInfo		bindingTargetTypeInfo;
			vint								generatedNameCount;
			ITypeDescriptor*					rootTypeDescriptor;

			WorkflowReferenceNamesVisitor(Ptr<GuiInstanceContext> _context, types::VariableTypeInfoMap& _typeInfos, types::ErrorList& _errors)
				:context(_context)
				, typeInfos(_typeInfos)
				, errors(_errors)
				, generatedNameCount(0)
				, rootTypeDescriptor(0)
			{
			}

			void Visit(GuiTextRepr* repr)override
			{
			}

			void Visit(GuiAttSetterRepr* repr)override
			{
				auto reprTypeInfo = bindingTargetTypeInfo;
				auto loader = GetInstanceLoaderManager()->GetLoader(reprTypeInfo.typeName);

				if (repr->instanceName != GlobalStringKey::Empty && reprTypeInfo.typeDescriptor)
				{
					if (typeInfos.Keys().Contains(repr->instanceName))
					{
						errors.Add(L"Precompile: Parameter \"" + repr->instanceName.ToString() + L"\" conflict with an existing named object.");
					}
					else
					{
						typeInfos.Add(repr->instanceName, reprTypeInfo);
					}
				}
				
				FOREACH_INDEXER(Ptr<GuiAttSetterRepr::SetterValue>, setter, index, repr->setters.Values())
				{
					IGuiInstanceLoader::TypeInfo propertyTypeInfo;

					if (setter->binding != GlobalStringKey::Empty && setter->binding != GlobalStringKey::_Set)
					{
						auto binder = GetInstanceLoaderManager()->GetInstanceBinder(setter->binding);
						if (!binder)
						{
							errors.Add(L"The appropriate IGuiInstanceBinder of binding \"" + setter->binding.ToString() + L"\" cannot be found.");
						}
						else if (repr->instanceName == GlobalStringKey::Empty && reprTypeInfo.typeDescriptor)
						{
							auto name = GlobalStringKey::Get(L"<precompile>" + itow(generatedNameCount++));
							repr->instanceName = name;
							typeInfos.Add(name, reprTypeInfo);
						}
					}

					if (setter->binding == GlobalStringKey::_Set)
					{
						IGuiInstanceLoader::PropertyInfo info;
						info.typeInfo = reprTypeInfo;
						info.propertyName = repr->setters.Keys()[index];
						auto currentLoader = loader;

						while (currentLoader)
						{
							auto typeInfo = currentLoader->GetPropertyType(info);
							if (typeInfo && typeInfo->support != GuiInstancePropertyInfo::NotSupport)
							{
								propertyTypeInfo.typeDescriptor = typeInfo->acceptableTypes[0];
								propertyTypeInfo.typeName = GlobalStringKey::Get(typeInfo->acceptableTypes[0]->GetTypeName());
								break;
							}
							currentLoader = GetInstanceLoaderManager()->GetParentLoader(currentLoader);
						}
					}

					FOREACH(Ptr<GuiValueRepr>, value, setter->values)
					{
						bindingTargetTypeInfo = propertyTypeInfo;
						value->Accept(this);
					}
				}

				FOREACH(Ptr<GuiAttSetterRepr::EventValue>, handler, repr->eventHandlers.Values())
				{
					if (handler->binding != GlobalStringKey::Empty)
					{
						auto binder = GetInstanceLoaderManager()->GetInstanceEventBinder(handler->binding);
						if (!binder)
						{
							errors.Add(L"The appropriate IGuiInstanceEventBinder of binding \"" + handler->binding.ToString() + L"\" cannot be found.");
						}
					}

					if (repr->instanceName == GlobalStringKey::Empty && reprTypeInfo.typeDescriptor)
					{
						auto name = GlobalStringKey::Get(L"<precompile>" + itow(generatedNameCount++));
						repr->instanceName = name;
						typeInfos.Add(name, reprTypeInfo);
					}
				}
			}

			void Visit(GuiConstructorRepr* repr)override
			{
				bool found = false;
				if (repr == context->instance.Obj() && context->className)
				{
					auto fullName = GlobalStringKey::Get(context->className.Value());
					auto td = GetInstanceLoaderManager()->GetTypeDescriptorForType(fullName);
					if (td)
					{
						found = true;
						bindingTargetTypeInfo.typeName = fullName;
						bindingTargetTypeInfo.typeDescriptor = td;
					}
				}

				if (!found)
				{
					auto source = FindInstanceLoadingSource(context, repr);
					bindingTargetTypeInfo.typeName = source.typeName;
					bindingTargetTypeInfo.typeDescriptor = GetInstanceLoaderManager()->GetTypeDescriptorForType(source.typeName);
				}

				if (!bindingTargetTypeInfo.typeDescriptor)
				{
					errors.Add(
						L"Precompile: Failed to find type \"" +
						(repr->typeNamespace == GlobalStringKey::Empty
							? repr->typeName.ToString()
							: repr->typeNamespace.ToString() + L":" + repr->typeName.ToString()
							) +
						L"\".");
				}

				if (context->instance.Obj() == repr)
				{
					rootTypeDescriptor = bindingTargetTypeInfo.typeDescriptor;
				}
				Visit((GuiAttSetterRepr*)repr);
			}
		};

/***********************************************************************
WorkflowCompileVisitor
***********************************************************************/

		class WorkflowCompileVisitor : public Object, public GuiValueRepr::IVisitor
		{
		public:
			Ptr<GuiInstanceContext>				context;
			types::VariableTypeInfoMap&			typeInfos;
			description::ITypeDescriptor*		rootTypeDescriptor;
			Ptr<WfBlockStatement>				statements;
			types::ErrorList&					errors;

			WorkflowCompileVisitor(Ptr<GuiInstanceContext> _context, types::VariableTypeInfoMap& _typeInfos, description::ITypeDescriptor* _rootTypeDescriptor, types::ErrorList& _errors)
				:context(_context)
				, typeInfos(_typeInfos)
				, rootTypeDescriptor(_rootTypeDescriptor)
				, errors(_errors)
				, statements(MakePtr<WfBlockStatement>())
			{
			}

			void Visit(GuiTextRepr* repr)override
			{
			}

			void Visit(GuiAttSetterRepr* repr)override
			{
				IGuiInstanceLoader::TypeInfo reprTypeInfo;
				if (repr->instanceName != GlobalStringKey::Empty)
				{
					reprTypeInfo = typeInfos[repr->instanceName];
				}

				FOREACH_INDEXER(Ptr<GuiAttSetterRepr::SetterValue>, setter, index, repr->setters.Values())
				{
					if (reprTypeInfo.typeDescriptor)
					{
						GlobalStringKey propertyName = repr->setters.Keys()[index];
						Ptr<GuiInstancePropertyInfo> propertyInfo;
						IGuiInstanceLoader::PropertyInfo info;
						info.typeInfo = reprTypeInfo;
						info.propertyName = propertyName;

						{
							auto currentLoader = GetInstanceLoaderManager()->GetLoader(info.typeInfo.typeName);

							while (currentLoader && !propertyInfo)
							{
								propertyInfo = currentLoader->GetPropertyType(info);
								if (propertyInfo && propertyInfo->support == GuiInstancePropertyInfo::NotSupport)
								{
									propertyInfo = 0;
								}
								currentLoader = GetInstanceLoaderManager()->GetParentLoader(currentLoader);
							}
						}

						if (!propertyInfo)
						{
							errors.Add(L"Precompile: Cannot find property \"" + propertyName.ToString() + L"\" in type \"" + reprTypeInfo.typeName.ToString() + L"\".");
						}
						else if (setter->binding != GlobalStringKey::Empty && setter->binding != GlobalStringKey::_Set)
						{
							WString expressionCode;
							if (auto obj = setter->values[0].Cast<GuiTextRepr>())
							{
								expressionCode = obj->text;
							}
							else
							{
								errors.Add(L"Precompile: Binder \"" + setter->binding.ToString() + L"\" requires the text value of property \"" + propertyName.ToString() + L"\".");
							}

							auto binder = GetInstanceLoaderManager()->GetInstanceBinder(setter->binding);
							if (binder)
							{
								auto instancePropertyInfo = info.typeInfo.typeDescriptor->GetPropertyByName(info.propertyName.ToString(), true);
								if (instancePropertyInfo)
								{
									if (auto statement = binder->GenerateInstallStatement(repr->instanceName, instancePropertyInfo, expressionCode, errors))
									{
										if (Workflow_ValidateStatement(context, typeInfos, rootTypeDescriptor, errors, expressionCode, statement))
										{
											statements->statements.Add(statement);	
										}
									}
								}
								else
								{
									errors.Add(L"Precompile: Binder \"" + setter->binding.ToString() + L"\" requires property \"" + propertyName.ToString() + L"\" to physically appear in type \"" + reprTypeInfo.typeName.ToString() + L"\".");
								}
							}
							else
							{
								errors.Add(L"The appropriate IGuiInstanceBinder of binding \"" + setter->binding.ToString() + L"\" cannot be found.");
							}
						}
					}

					FOREACH(Ptr<GuiValueRepr>, value, setter->values)
					{
						value->Accept(this);
					}
				}

				FOREACH_INDEXER(Ptr<GuiAttSetterRepr::EventValue>, handler, index, repr->eventHandlers.Values())
				{
					if (reprTypeInfo.typeDescriptor)
					{
						GlobalStringKey propertyName = repr->eventHandlers.Keys()[index];
						auto td = reprTypeInfo.typeDescriptor;
						auto eventInfo = td->GetEventByName(propertyName.ToString(), true);

						if (!eventInfo)
						{
							errors.Add(L"Precompile: Event \"" + propertyName.ToString() + L"\" cannot be found in type \"" + reprTypeInfo.typeName.ToString() + L"\".");
						}
						else
						{
							Ptr<WfStatement> statement;

							if (handler->binding == GlobalStringKey::Empty)
							{
								statement = Workflow_InstallEvent(repr->instanceName, eventInfo, handler->value);
							}
							else
							{
								auto binder = GetInstanceLoaderManager()->GetInstanceEventBinder(handler->binding);
								if (binder)
								{
									statement = binder->GenerateInstallStatement(repr->instanceName, eventInfo, handler->value, errors);
								}
								else
								{
									errors.Add(L"The appropriate IGuiInstanceEventBinder of binding \"" + handler->binding.ToString() + L"\" cannot be found.");
								}
							}

							if (statement)
							{
								if (Workflow_ValidateStatement(context, typeInfos, rootTypeDescriptor, errors, handler->value, statement))
								{
									statements->statements.Add(statement);
								}
							}
						}
					}
				}
			}

			void Visit(GuiConstructorRepr* repr)override
			{
				Visit((GuiAttSetterRepr*)repr);
			}
		};

/***********************************************************************
Workflow_PrecompileInstanceContext
***********************************************************************/

		WString Workflow_ModuleToString(Ptr<workflow::WfModule> module)
		{
			stream::MemoryStream stream;
			{
				stream::StreamWriter writer(stream);
				WfPrint(module, L"", writer);
			}
			stream.SeekFromBegin(0);
			stream::StreamReader reader(stream);
			return reader.ReadToEnd();
		}

		Ptr<workflow::runtime::WfAssembly> Workflow_PrecompileInstanceContext(Ptr<GuiInstanceContext> context, types::ErrorList& errors)
		{
			ITypeDescriptor* rootTypeDescriptor = 0;
			types::VariableTypeInfoMap typeInfos;
			{
				FOREACH(Ptr<GuiInstanceParameter>, parameter, context->parameters)
				{
					auto type = GetTypeDescriptor(parameter->className.ToString());
					if (!type)
					{
						errors.Add(L"Precompile: Cannot find type \"" + parameter->className.ToString() + L"\".");
					}
					else if (typeInfos.Keys().Contains(parameter->name))
					{
						errors.Add(L"Precompile: Parameter \"" + parameter->name.ToString() + L"\" conflict with an existing named object.");
					}
					else
					{
						IGuiInstanceLoader::TypeInfo typeInfo;
						typeInfo.typeDescriptor = type;
						typeInfo.typeName = GlobalStringKey::Get(type->GetTypeName());
						typeInfos.Add(parameter->name, typeInfo);
					}
				}

				WorkflowReferenceNamesVisitor visitor(context, typeInfos, errors);
				context->instance->Accept(&visitor);
				rootTypeDescriptor = visitor.rootTypeDescriptor;
			}

			if (errors.Count() == 0)
			{
				WorkflowCompileVisitor visitor(context, typeInfos, rootTypeDescriptor, errors);
				context->instance->Accept(&visitor);
				auto module = Workflow_CreateModuleWithInitFunction(context, typeInfos, rootTypeDescriptor, visitor.statements);

				Workflow_GetSharedManager()->Clear(true, true);
				Workflow_GetSharedManager()->AddModule(module);
				Workflow_GetSharedManager()->Rebuild(true);
				WString moduleCode = Workflow_ModuleToString(module);

				if (Workflow_GetSharedManager()->errors.Count() == 0)
				{
					return GenerateAssembly(Workflow_GetSharedManager());
				}
				else
				{
					errors.Add(ERROR_CODE_PREFIX L"Unexpected errors are encountered when initializing data binding.");
					FOREACH(Ptr<parsing::ParsingError>, error, Workflow_GetSharedManager()->errors)
					{
						errors.Add(error->errorMessage);
					}
					errors.Add(ERROR_CODE_PREFIX L"Print code for reference:");
					errors.Add(moduleCode);
				}
			}

			return nullptr;
		}

/***********************************************************************
Workflow_RunPrecompiledScript
***********************************************************************/

		void Workflow_RunPrecompiledScript(Ptr<GuiInstanceEnvironment> env)
		{
			auto compiled = env->resolver->ResolveResource(L"res", L"Precompiled/Workflow/InstanceCtor/" + env->path).Cast<GuiInstanceCompiledWorkflow>();
			auto globalContext = MakePtr<WfRuntimeGlobalContext>(compiled->assembly);
				
			try
			{
				LoadFunction<void()>(globalContext, L"<initialize>")();
			}
			catch (const TypeDescriptorException& ex)
			{
				env->scope->errors.Add(L"Workflow Script Exception: " + ex.Message());
			}

			Workflow_SetVariablesForReferenceValues(globalContext, env);

			try
			{
				LoadFunction<void(Value, Ptr<GuiResourcePathResolver>)>(globalContext, L"<initialize-instance>")(env->scope->rootInstance, env->resolver);
			}
			catch (const TypeDescriptorException& ex)
			{
				env->scope->errors.Add(L"Workflow Script Exception: " + ex.Message());
			}
		}

/***********************************************************************
GuiWorkflowSharedManagerPlugin
***********************************************************************/

#undef ERROR_CODE_PREFIX

		class GuiWorkflowSharedManagerPlugin;
		GuiWorkflowSharedManagerPlugin* sharedManagerPlugin = 0;

		class GuiWorkflowSharedManagerPlugin : public Object, public IGuiPlugin
		{
		protected:
			Ptr<WfLexicalScopeManager>		workflowManager;

		public:
			GuiWorkflowSharedManagerPlugin()
			{
			}

			void Load()override
			{
			}

			void AfterLoad()override
			{
				sharedManagerPlugin = this;
			}

			void Unload()override
			{
				sharedManagerPlugin = 0;
			}

			WfLexicalScopeManager* GetWorkflowManager()
			{
				if (!workflowManager)
				{
					workflowManager = new WfLexicalScopeManager(GetParserManager()->GetParsingTable(L"WORKFLOW"));
				}
				return workflowManager.Obj();
			}
		};
		GUI_REGISTER_PLUGIN(GuiWorkflowSharedManagerPlugin)

		WfLexicalScopeManager* Workflow_GetSharedManager()
		{
			return sharedManagerPlugin->GetWorkflowManager();
		}
	}
}