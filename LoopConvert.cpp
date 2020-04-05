/*includes*/
#include "/storage/caos/akshayb/akshay/llvm-install/include/clang/Frontend/FrontendActions.h"
#include "/storage/caos/akshayb/akshay/llvm-install/include/clang/Tooling/CommonOptionsParser.h"
#include "/storage/caos/akshayb/akshay/llvm-install/include/clang/Tooling/Tooling.h"
#include "/storage/caos/akshayb/akshay/llvm-install/include/llvm/Support/CommandLine.h"
#include "/storage/caos/akshayb/akshay/llvm-install/include/clang/Frontend/CompilerInstance.h"
#include "/storage/caos/akshayb/akshay/llvm-install/include/clang/ASTMatchers/ASTMatchFinder.h"
#include "/storage/caos/akshayb/akshay/llvm-install/include/clang/ASTMatchers/ASTMatchers.h"
#include "/storage/caos/akshayb/akshay/llvm-install/include/clang/Rewrite/Core/Rewriter.h"
#include "/storage/caos/akshayb/akshay/llvm-install/include/clang/Lex/Lexer.h"
#include "/storage/caos/akshayb/akshay/llvm-install/include/clang/Sema/Sema.h"
#include <string>
#include <iostream>
#include <fstream>
#include <unordered_map>
#include <vector>
#include <sstream>

using namespace std;
using namespace llvm;
using namespace clang;
using namespace clang::tooling;
using namespace clang::SrcMgr;
using namespace clang::tooling;
using namespace clang::ast_matchers;
using namespace clang::driver;

clang::LangOptions lopt;
//This is shown when someone uses --help with your tool.
static llvm::cl::OptionCategory MyToolCategory("my-tool options");
static llvm::cl::OptionCategory MyToolCategory1("my-tool options1");
static cl::extrahelp CommonHelp(CommonOptionsParser::HelpMessage);
static cl::extrahelp MoreHelp("\nMore help text...\n");


void hoister_fun();
/*datastructures*/
unordered_map<int,vector<pair<string,string>>>  varinfo;
unordered_map<string,vector<int>> arrinfo; //7 a[4][5][6] -> <a7,<3,6->5->4>>
unordered_map<string, int> vardepth;//varname, depth of label/fun in which it lies.
unordered_map<int,string> scopeinit;
unordered_map<int,string> scopeinfo;
unordered_map<int,string> scopeinfo_str;
unordered_map<int,string> nameinfo; //line no, name
unordered_map<int,int> depthinfo; //line no,depth
unordered_map<int,string> renameinfo; //line no, newname
unordered_map<int,vector<int>> depthtrav; // line no, parent traversal list
unordered_map<int,int> rangeinfo; // contains range information of given function/label
unordered_map<string,vector<pair<int,int>>> callresinfo; //data required for call resolution information 
unordered_map<int,int> callres; //actual call resolution information
unordered_map<string,int> refdepth; // where to read the scope from for given variable
unordered_map<int, int> callexprdepth; // stores information of depth of function from where function is called + 1
unordered_map<string,int> globalfun; //if function is global function, contains it's line no.
unordered_map<int,string> prototypes; //contains prototypes of all nested labels
unordered_map<string,string> lab_parent; //label_name -> parent
unordered_map<string,string> orig_list; //original view of function/label before hoisting (fun_name -> view)
unordered_map<string,string> updated_list; //updated view of function/label for hoisting (fun_name -> view)
/*Data structures for structure renaming and hoisting.*/
unordered_map<string,string> struct_hoist_info;
map<int,string> struct_hoist_info_ord;
unordered_map<int,unordered_map<string,int>> str_parent;
unordered_map<int,int> str_visited;
unordered_map<string,int> str_var_visited;

int get_line_no(string line)
{
    size_t b = line.find(':');
    size_t e = line.find(':',b+1);
            
    line = line.substr(b+1, e-b-1);
    return stoi(line);
}

class VarHandler : public MatchFinder::MatchCallback
{
	private:
	Rewriter& R;

	public:
	VarHandler(Rewriter& Rewrite): R(Rewrite)  {}
	
	virtual void run(const MatchFinder::MatchResult &Result)
	{

		if(const VarDecl* vd  = Result.Nodes.getNodeAs<clang::VarDecl>("var"))
		{
			//handle variables declared in function
		 	if(const FunctionDecl *fdecl = Result.Nodes.getNodeAs<clang::FunctionDecl>("fun"))
			{
		   	string line = fdecl->getBeginLoc().printToString(R.getSourceMgr());
				int l = get_line_no(line);
			
				if(varinfo.find(l) == varinfo.end())
				{	
					varinfo[l] = vector<pair<string,string>>();
					varinfo[l].push_back(make_pair(fdecl->getNameAsString()," "));
				}				
				varinfo[l].push_back(make_pair(vd->getNameAsString(),vd->getType().getAsString()));

				//cout<<fdecl->getName().bytes_begin()<<" "<<l<<endl;
				
				/*Update vardepth*/
				line = vd->getBeginLoc().printToString(R.getSourceMgr());
				int var_line = get_line_no(line);

				stringstream ss;
				ss<<vd->getNameAsString()<<var_line;
				string varname = ss.str();
				vardepth[varname] =  depthinfo[l];


				/*update Scope*/
			   if(scopeinfo.find(l) == scopeinfo.end())
				{
					stringstream ss;
					ss << "struct "<<"s_"<<fdecl->getNameAsString()<<"{"<<endl<<"};";

					scopeinfo[l] = ss.str(); 	
				}

				/*insert into string*/
				string type;
				if(vd->getType().getAsString().find("int") != string::npos)
				{
						  type = "int";
				}
				else if(vd->getType().getAsString().find("float") != string::npos)
				{
						  type = "float";
				}
				else if(vd->getType().getAsString().find("struct") != string::npos)
				{
						if(vd->getType().getAsString().find("[") != string::npos)
						{
								type = vd->getType().getAsString();
								type.erase(vd->getType().getAsString().find("["), vd->getType().getAsString().length());
						}
						else
						{
								  type = vd->getType().getAsString();
						}
				}
				else if(vd->getType().getAsString().find("char") != string::npos)
				{
						  type = "char";
				}


				ss.str(string());
				ss <<type<<"* "<<vd->getNameAsString()<<";"<<endl;
				scopeinfo[l].insert((scopeinfo[l].length()-2), ss.str());
	
				/*Update scope init*/
				if(scopeinit.find(l) == scopeinit.end())
				{
					stringstream ss;
					ss<<endl<<"struct s_"<<fdecl->getNameAsString()<<" my_s;"<<endl;
					scopeinit[l] = ss.str();
				}
				
				ss.str(string()); //clear ss
				ss<<"my_s."<<vd->getNameAsString()<<" = &"<<vd->getNameAsString()<<";"<<endl;
	
				scopeinit[l].insert(scopeinit[l].length(), ss.str());

				if(vd->getType().getAsString().find("[") != string::npos)
				{
					if(arrinfo.find(varname) == arrinfo.end())
					{
						arrinfo[varname] = vector<int>();
						int dim = count(vd->getType().getAsString(), '[');
						
					//	cout<<varname<<" "<<"dim: "<< dim<<endl;
						string line = vd->getType().getAsString();					
						size_t b = 0;
						size_t e;
						int x;
						for(int k=0;k<dim;k++)
						{
							   b = line.find('[',b);
								e = line.find(']',b+1);

								//line = line.substr(b+1, e-b-1);
							   x = stoi(line.substr(b+1,e-b-1));
								arrinfo[varname].push_back(x);
								b = e+1;
						}
					}
				}

			}

			//handle variables declared in Labels
		 	if(const LabelStmt *ls = Result.Nodes.getNodeAs<clang::LabelStmt>("label"))
			{
		   	string line = ls->getBeginLoc().printToString(R.getSourceMgr());
				int l = get_line_no(line);
			
				if(varinfo.find(l) == varinfo.end())
				{	
					varinfo[l] = vector<pair<string,string>>();
					varinfo[l].push_back(make_pair(string(ls->getName())," "));
				}				
				varinfo[l].push_back(make_pair(vd->getNameAsString(),vd->getType().getAsString()));
			
				/*Update vardepth*/
				line = vd->getBeginLoc().printToString(R.getSourceMgr());
				int var_line = get_line_no(line);

				stringstream ss;
				ss<<vd->getNameAsString()<<var_line;
				string varname = ss.str();
				//cout<<"updating2 :"<<varname<<endl;
				vardepth[varname] =  depthinfo[l];

				/*update Scope*/
			   if(scopeinfo.find(l) == scopeinfo.end())
				{
					stringstream ss;
					ss <<endl<< "struct "<<"s_"<<renameinfo[l] <<"{"<<endl<<"struct s_"<<renameinfo[depthtrav[l][1]]<<" *s;"<<endl<<"};";

					scopeinfo[l] = ss.str(); 	
				}

				/*insert into string*/
				string type;
				if(vd->getType().getAsString().find("int") != string::npos)
				{
						  type = "int";
				}
				else if(vd->getType().getAsString().find("float") != string::npos)
				{
						  type = "float";
				}
				else if(vd->getType().getAsString().find("struct") != string::npos)
				{
						if(vd->getType().getAsString().find("[") != string::npos)
						{
								type = vd->getType().getAsString();
								type.erase(vd->getType().getAsString().find("["), vd->getType().getAsString().length());
						}
						else
						{
								  type = vd->getType().getAsString();
						}
	
				}
				else if(vd->getType().getAsString().find("char") != string::npos)
				{
						  type = "char";
				}

				ss.str(string());
				ss <<type<<"* "<<vd->getNameAsString()<<";"<<endl;
				scopeinfo[l].insert((scopeinfo[l].length()-2), ss.str());				
				
				/*Update scope init*/
				if(scopeinit.find(l) == scopeinit.end())
				{
					stringstream ss;
					ss<<endl<<"struct s_"<<renameinfo[l]<<" my_s;"<<endl;
					ss<<"my_s.s = par_s;"<<endl;
					scopeinit[l] = ss.str();
				}
				
				ss.str(string()); //clear ss
				ss<<scopeinit[l]<<"my_s."<<vd->getNameAsString()<<" = &"<<vd->getNameAsString()<<";"<<endl;
	
				scopeinit[l] = ss.str();//.insert(scopeinit[l].length(), ss.str());
				if(vd->getType().getAsString().find("[") != string::npos)
				{
					if(arrinfo.find(varname) == arrinfo.end())
					{
						arrinfo[varname] = vector<int>();
						int dim = count(vd->getType().getAsString(), '[');
						
						//cout<<varname<<" "<<"dim: "<< dim<<endl;
						string line = vd->getType().getAsString();					
						size_t b = 0;
						size_t e;
						int x;
						for(int k=0;k<dim;k++)
						{
							   b = line.find('[',b);
								e = line.find(']',b+1);

								//line = line.substr(b+1, e-b-1);
							   x = stoi(line.substr(b+1,e-b-1));
								arrinfo[varname].push_back(x);
								b = e+1;
						}
					}
				}


			}
		}

		/*Functions and labels which don't have variable declarations are handled seperately.*/
		/*Function with labels inside*/
		if(const FunctionDecl *fdecl = Result.Nodes.getNodeAs<clang::FunctionDecl>("fun"))
		{
			/*Doesn't make sense to have scope, but still including it to avoid complexity.*/
		
	   	 string line = fdecl->getBeginLoc().printToString(R.getSourceMgr());
			 int l = get_line_no(line);

			 stringstream ss;
			 if(scopeinfo.find(l) == scopeinfo.end())
			 {
				 ss << endl<<"struct "<<"s_"<<fdecl->getNameAsString()<<"{"<<endl<<"};";
		
				 scopeinfo[l] = ss.str(); 	
			 }
		
			 /*Update scope init*/
			 if(scopeinit.find(l) == scopeinit.end())
			 {
				 stringstream ss;
				 ss<<endl<<"struct s_"<<fdecl->getNameAsString()<<" my_s;"<<endl;
				 scopeinit[l] = ss.str();
			 }
		}	
		/*And all labels*/	
		if(const LabelStmt *ls = Result.Nodes.getNodeAs<clang::LabelStmt>("lab"))
		{
		  	string line = ls->getBeginLoc().printToString(R.getSourceMgr());
			int l = get_line_no(line);
		
			stringstream ss;
			/*update Scope*/
		   if(scopeinfo.find(l) == scopeinfo.end())
			{
				stringstream ss;
				ss << "struct "<<"s_"<<renameinfo[l] <<"{"<<endl<<"struct s_"<<renameinfo[depthtrav[l][1]]<<" *s;"<<endl<<"};";
				scopeinfo[l] = ss.str(); 	
			}

			/*Update scope init*/
			if(scopeinit.find(l) == scopeinit.end())
			{
				stringstream ss;
				ss<<"struct s_"<<renameinfo[l]<<" my_s;"<<endl;
				ss<<"my_s.s = par_s;"<<endl;
				scopeinit[l] = ss.str();
			}
			
			/*update prototypes*/
			ss<<"void "<<renameinfo[l]<<"(struct s_"<<renameinfo[depthtrav[l][1]]<<" *par_s)";
			if(prototypes.find(l) == prototypes.end())
			{
				prototypes[l] = ss.str();
			}	
		}
	}
};

class CallResHandler : public MatchFinder::MatchCallback
{
	private:
	Rewriter& R;

	public:
	CallResHandler(Rewriter& Rewrite): R(Rewrite)  {}
	
	virtual void run(const MatchFinder::MatchResult &Result)
	{
		if(const FunctionDecl* fdecl = Result.Nodes.getNodeAs<clang::FunctionDecl>("funlist"))
		{
			string line = fdecl->getBeginLoc().printToString(R.getSourceMgr());
         int l = get_line_no(line);
			
			globalfun[fdecl->getNameAsString()] = l; 		
		}

	   /*Handle Depth 1 list*/	
		if(const FunctionDecl* fdecl = Result.Nodes.getNodeAs<clang::FunctionDecl>("fun"))
		{
		 	if(const LabelStmt* ls  = Result.Nodes.getNodeAs<clang::LabelStmt>("childlabel"))
			{
            string line = ls->getBeginLoc().printToString(R.getSourceMgr());
            int lab_line = get_line_no(line);
				line = ls->getEndLoc().printToString(R.getSourceMgr()); 
				int lab_e_line = rangeinfo[depthtrav[lab_line][1]];
				
				if(callresinfo.find(string(ls->getName())) == callresinfo.end())
				{
					callresinfo[string(ls->getName())] = vector<pair<int,int>>();
				}
				callresinfo[string(ls->getName())].push_back(make_pair(lab_line,lab_e_line));

			}
		}
		
		/*Handle other depths*/
      if(const LabelStmt* pl = Result.Nodes.getNodeAs<clang::LabelStmt>("parlabel"))
      {
         if(const LabelStmt* cl  = Result.Nodes.getNodeAs<clang::LabelStmt>("childlabel"))
         {
           // string line = pl->getBeginLoc().printToString(R.getSourceMgr());
           // int pl_line = get_line_no(line);

            string line = cl->getBeginLoc().printToString(R.getSourceMgr());
            int cl_line = get_line_no(line);
            line = cl->getEndLoc().printToString(R.getSourceMgr());
            int cl_e_line = rangeinfo[depthtrav[cl_line][1]];;
			
				if(callresinfo.find(string(cl->getName())) == callresinfo.end())
				{
					callresinfo[string(cl->getName())] = vector<pair<int,int>>();
				}
				callresinfo[string(cl->getName())].push_back(make_pair(cl_line,cl_e_line));
          }
       }

		if(const CallExpr* ce = Result.Nodes.getNodeAs<clang::CallExpr>("call"))
		{

			// check if call expression is valid
			// if valid, assign default depth of 1
			// and store the line no of label that following call resolves to
			string line = ce->getBeginLoc().printToString(R.getSourceMgr());
			int l = get_line_no(line);

			string name = ce->getDirectCallee()->getNameInfo().getName().getAsString();
		//	cout << name<<endl;
			int found = 0;
			if(callresinfo.find(name) != callresinfo.end())
			{
				for(auto i:callresinfo[name])
				{
					if((i.first < l) && (i.second >= l))
					{
						callres[l] = i.first;
						callexprdepth[l] = 0; //default depth
						found = 1;
						break;					
					}
				}
			}
			if((found == 0) && (globalfun.find(name) == globalfun.end()))
			{
				cout << "error:"<<l<<":"<<name<<": unresolved function"<<endl;
			}
			else if(const LabelStmt* ls = Result.Nodes.getNodeAs<clang::LabelStmt>("lab")) 
			{					 
				if(globalfun.find(name) == globalfun.end())
				{
					string line = ls->getBeginLoc().printToString(R.getSourceMgr());
					int lab_line = get_line_no(line);

					// a = depth of call expr i.e parent label's depth +1
					// b = depth of corresponding definition
					// callexprdepth = a - b
					callexprdepth[l] = (depthinfo[lab_line] + 1) - depthinfo[callres[l]]; 
				}
			}
		}	
 	}
};

class DepthHandler : public MatchFinder::MatchCallback
{
	private:
	Rewriter& R;

	public:
	DepthHandler(Rewriter& Rewrite): R(Rewrite)  {}
	
	virtual void run(const MatchFinder::MatchResult &Result)
	{

		if(const FunctionDecl* fdecl = Result.Nodes.getNodeAs<clang::FunctionDecl>("funlist"))
		{
			string line = fdecl->getBeginLoc().printToString(R.getSourceMgr());
         int l = get_line_no(line);
			line = fdecl->getEndLoc().printToString(R.getSourceMgr());			
			int end_l = get_line_no(line);

			rangeinfo[l] = end_l;
			depthinfo[l] = 0;
			nameinfo[l] = fdecl->getNameAsString(); 
			renameinfo[l] = fdecl->getNameAsString();
			depthtrav[l] = vector<int>();	
			depthtrav[l].push_back(l);	
		}

	   /*Handle Depth 1 list*/	
		if(const FunctionDecl* fdecl = Result.Nodes.getNodeAs<clang::FunctionDecl>("fun"))
		{
		 	if(const LabelStmt* ls  = Result.Nodes.getNodeAs<clang::LabelStmt>("childlabel"))
			{
            string line = ls->getBeginLoc().printToString(R.getSourceMgr());
            int lab_line = get_line_no(line);
				line = ls->getEndLoc().printToString(R.getSourceMgr()); 
				int lab_e_line = get_line_no(line);

				rangeinfo[lab_line] = lab_e_line;
			
		      line = fdecl->getBeginLoc().printToString(R.getSourceMgr());
            int fun_line = get_line_no(line);
				
				depthinfo[lab_line] = 1;
				nameinfo[lab_line] = string(ls->getName());
				
				stringstream ss;
				ss << nameinfo[fun_line] << "_" << nameinfo[lab_line];
				renameinfo[lab_line] = ss.str();
				
				depthtrav[lab_line] = vector<int>();
				depthtrav[lab_line].push_back(lab_line);
				depthtrav[lab_line].push_back(fun_line);
			}
		}
		
		/*Handle other depths*/
      if(const LabelStmt* pl = Result.Nodes.getNodeAs<clang::LabelStmt>("parlabel"))
      {
         if(const LabelStmt* cl  = Result.Nodes.getNodeAs<clang::LabelStmt>("childlabel"))
         {
            string line = pl->getBeginLoc().printToString(R.getSourceMgr());
            int pl_line = get_line_no(line);

            line = cl->getBeginLoc().printToString(R.getSourceMgr());
            int cl_line = get_line_no(line);
            line = cl->getEndLoc().printToString(R.getSourceMgr());
            int cl_e_line = get_line_no(line);
				rangeinfo[cl_line] = cl_e_line;            

            depthinfo[cl_line] = depthinfo[pl_line] + 1;
            nameinfo[cl_line] = string(cl->getName());
           
            stringstream ss;
            ss << renameinfo[pl_line] << "_" << nameinfo[cl_line];
            renameinfo[cl_line] = ss.str();
            
            depthtrav[cl_line] = vector<int>();
            depthtrav[cl_line].push_back(cl_line);
            depthtrav[cl_line].insert(depthtrav[cl_line].end(), depthtrav[pl_line].begin(), depthtrav[pl_line].end());
          }
       }
	}
};

class ExpHandler : public MatchFinder::MatchCallback
{
	private:
	Rewriter& R;

	public:
	ExpHandler(Rewriter& Rewrite): R(Rewrite)  {}
	
	virtual void run(const MatchFinder::MatchResult &Result)
	{
		if(const DeclRefExpr* dl = Result.Nodes.getNodeAs<clang::DeclRefExpr>("exp"))
		{
			if(const LabelStmt* ls = Result.Nodes.getNodeAs<clang::LabelStmt>("lab"))
			{
				if(dl->getValueKind() !=0)
				{
					stringstream ss;

					/*use_line and use_name*/
					string line = dl->getBeginLoc().printToString(R.getSourceMgr());
					int use_l = get_line_no(line);

					ss<<dl->getFoundDecl()->getNameAsString()<<use_l;
					string use_index = ss.str();

					/*decl_line and decl_name*/
					line = dl->getFoundDecl()->getBeginLoc().printToString(R.getSourceMgr());
					int decl_l = get_line_no(line);

					stringstream ssd;
					ssd<<dl->getFoundDecl()->getNameAsString()<<decl_l;
					string decl_index = ssd.str();

					/*label line no*/
					line = ls->getBeginLoc().printToString(R.getSourceMgr());
					int label_line = get_line_no(line);

					int par_depth = depthinfo[label_line];

					/*reference depth*/
					if(vardepth.find(decl_index) != vardepth.end())
					{
						refdepth[use_index] = par_depth - vardepth[decl_index];
					}
				}
			}
		}

	}
};

class CallExpRewriter : public MatchFinder::MatchCallback
{
	private:
	Rewriter& R;

	public:
	CallExpRewriter(Rewriter& Rewrite): R(Rewrite)  {}
	
	virtual void run(const MatchFinder::MatchResult &Result)
	{
		if(const CallExpr* ce = Result.Nodes.getNodeAs<clang::CallExpr>("call"))
		{
			string line = ce->getBeginLoc().printToString(R.getSourceMgr());
			int l = get_line_no(line);
			string name = ce->getDirectCallee()->getNameInfo().getName().getAsString();
		
			if(callexprdepth.find(l) != callexprdepth.end())
			{
				stringstream arg;
				if(callexprdepth[l] == 0)
				{
					arg<< "&my_s";
				}
				else if(callexprdepth[l] == 1)
				{
					arg<< "par_s";
				}
				else
				{
					arg<<"par_s";
					for(int k=1;k<callexprdepth[l];k++)
					{
						arg<<"->s";
					}
				}
	
				stringstream call_exp;
				call_exp<<renameinfo[callres[l]]<<"("<<arg.str()<<")";
				R.InsertText(ce->getBeginLoc(),call_exp.str(),true);
				R.RemoveText(ce->getBeginLoc(),name.length() + 2);
			}	
		}
	}
};

class StructInit : public MatchFinder::MatchCallback
{
	private:
	Rewriter& R;

	public:
	StructInit(Rewriter& Rewrite): R(Rewrite)  {}
	
	virtual void run(const MatchFinder::MatchResult &Result)
	{
		if(const LabelStmt* ls = Result.Nodes.getNodeAs<clang::LabelStmt>("lab"))
		{
			string line = ls->getBeginLoc().printToString(R.getSourceMgr());
			int l = get_line_no(line);
			R.InsertTextBefore(ls->getBeginLoc(),renameinfo[l]);
			R.RemoveText(ls->getBeginLoc(),string(ls->getName()).length());
			R.InsertTextBefore(ls->getBeginLoc(),scopeinit[depthtrav[l][1]]);
			scopeinit[depthtrav[l][1]] = "";
		}
	}
};

class ExpressionRewriter : public MatchFinder::MatchCallback
{
	private:
	Rewriter& R;

	public:
	ExpressionRewriter(Rewriter& Rewrite): R(Rewrite)  {}
	
	virtual void run(const MatchFinder::MatchResult &Result)
	{
		if(const DeclRefExpr* dl = Result.Nodes.getNodeAs<clang::DeclRefExpr>("exp"))
		{
			if(const LabelStmt* ls = Result.Nodes.getNodeAs<clang::LabelStmt>("lab"))
			{
				if(dl->getValueKind() !=0)
				{
					stringstream ss;

					/*use_line and use_name*/
					string line = dl->getBeginLoc().printToString(R.getSourceMgr());
					int use_l = get_line_no(line);
					ss<<dl->getFoundDecl()->getNameAsString()<<use_l;
					string use_index = ss.str();

					string decl_line;
					decl_line = dl->getFoundDecl()->getBeginLoc().printToString(R.getSourceMgr());
					int decl_l = get_line_no(decl_line);

					stringstream decl_ss;
					decl_ss<<dl->getFoundDecl()->getNameAsString()<<decl_l;
					string decl_index = decl_ss.str();
				//	cout<<decl_index<<" for "<<use_index<<endl;
					if(arrinfo.find(decl_index) == arrinfo.end())
					{
						stringstream new_name;
						if((refdepth.find(use_index) != refdepth.end()) && (refdepth[use_index]!=0))
						{
							new_name<<"(*(par_s->";
							/*reference depth*/
							for(int k=1;k<refdepth[use_index];k++)
							{
								new_name<<"s->";
							}
							new_name<<dl->getFoundDecl()->getNameAsString()<<"))";

							R.InsertText(dl->getBeginLoc(),new_name.str(),true,true);
							R.RemoveText(dl->getBeginLoc(),dl->getFoundDecl()->getNameAsString().length());
						}
					}
					else/*array writing*/
					{
							  stringstream new_name;
							  if((refdepth.find(use_index) != refdepth.end()) && (refdepth[use_index] !=0))
							  {
									new_name<<"(par_s->";
									for(int k = 1; k<refdepth[use_index]; k++)
									{
											  new_name<<"s->";
									}
									new_name<<dl->getFoundDecl()->getNameAsString()<<")";

									SourceLocation loc = dl->getBeginLoc();
									R.InsertText(loc,new_name.str(),true,true);
									R.RemoveText(loc,dl->getFoundDecl()->getNameAsString().length());
									
									/*first opening square bracket*/
									int dim = arrinfo[decl_index].size()-1;
									loc = clang::Lexer::findNextToken(loc, R.getSourceMgr(), R.getLangOpts())->getLocation();
								//	cout<<clang::Lexer::findNextToken(loc, R.getSourceMgr(), R.getLangOpts())->getName();
									R.InsertTextAfterToken(loc,"(");

									int brace = 0;
									for(int f = arrinfo[decl_index].size()-1;f>0;f--)
									{
										brace = 0;
										
								/*	loc = clang::Lexer::findNextToken(loc, R.getSourceMgr(), R.getLangOpts())->getLocation();
									cout<<clang::Lexer::findNextToken(loc, R.getSourceMgr(), R.getLangOpts())->getName();

									loc = clang::Lexer::findNextToken(loc, R.getSourceMgr(), R.getLangOpts())->getLocation();
									cout<<clang::Lexer::findNextToken(loc, R.getSourceMgr(), R.getLangOpts())->getName();*/
										while(1)
										{
												  //cout<<clang::Lexer::findNextToken(loc, R.getSourceMgr(), R.getLangOpts())->getName()<<endl;
											if( strcmp(clang::Lexer::findNextToken(loc, R.getSourceMgr(), R.getLangOpts())->getName(),"l_square") == 0) 
											{
													  brace++;
													 // cout<<brace<<" :incremented\n";
											}
											else if(strcmp(clang::Lexer::findNextToken(loc, R.getSourceMgr(), R.getLangOpts())->getName(),"r_square") == 0)
											{
													  brace--;
													//  cout<<brace<<" :decremented\n";
											}
											loc = clang::Lexer::findNextToken(loc, R.getSourceMgr(), R.getLangOpts())->getLocation();
											if(brace == -1) break;
										}
										
										stringstream ss;
										ss<<")";
										for(int g=f-1;g>=0;g--)
										{
												  ss<<" * "<<arrinfo[decl_index][dim - g];
										}
										ss<<"+(";

										R.ReplaceText(loc,ss.str());
								 		loc = clang::Lexer::findNextToken(loc, R.getSourceMgr(), R.getLangOpts())->getLocation();
										R.ReplaceText(loc,"");
									}

									brace = 0 ;
										while(1)
										{
												  //cout<<clang::Lexer::findNextToken(loc, R.getSourceMgr(), R.getLangOpts())->getName()<<endl;
											if( strcmp(clang::Lexer::findNextToken(loc, R.getSourceMgr(), R.getLangOpts())->getName(),"l_square") == 0) 
											{
													  brace++;
													//  cout<<brace<<" :incremented\n";
											}
											else if(strcmp(clang::Lexer::findNextToken(loc, R.getSourceMgr(), R.getLangOpts())->getName(),"r_square") == 0)
											{
													  brace--;
													//  cout<<brace<<" :decremented\n";
											}
											loc = clang::Lexer::findNextToken(loc, R.getSourceMgr(), R.getLangOpts())->getLocation();
											if(brace == -1) break;
										}
									R.InsertTextBefore(loc,")");


							  }
					}

					/*array rewriting*/
					

				}
			}
		}
	}
};

//int b = 0;
class  Hoist_out : public MatchFinder::MatchCallback
{
	private:
	Rewriter& R;

	public:
	Hoist_out(Rewriter& Rewrite): R(Rewrite)  {}
	
	virtual void run(const MatchFinder::MatchResult &Result)
	{

		if(const FunctionDecl* fdecl = Result.Nodes.getNodeAs<clang::FunctionDecl>("funlist"))
		{
			string name = fdecl->getNameAsString();
			orig_list[name] = R.getRewrittenText(fdecl->getSourceRange());
			updated_list[name] = orig_list[name];
		}


 		if(const FunctionDecl* fdecl = Result.Nodes.getNodeAs<clang::FunctionDecl>("fun"))
		{
		 	if(const LabelStmt* ls  = Result.Nodes.getNodeAs<clang::LabelStmt>("childlabel"))
			{
				string name = string(ls->getName());
				lab_parent[name] = fdecl->getNameAsString();
				orig_list[name] = R.getRewrittenText(ls->getSourceRange());
				updated_list[name] = orig_list[name];
			}
		}
		
		/*Handle other depths*/
      if(const LabelStmt* pl = Result.Nodes.getNodeAs<clang::LabelStmt>("parlabel"))
      {
         if(const LabelStmt* cl  = Result.Nodes.getNodeAs<clang::LabelStmt>("childlabel"))
         {
				string name = string(cl->getName());
				lab_parent[name] = string(pl->getName());
				orig_list[name] = R.getRewrittenText(cl->getSourceRange());
				updated_list[name] = orig_list[name];	
         }
       }

	}
};

unordered_map<string,vector<string>> struct_list;
unordered_map<string,int> struct_list_visited;
class  StructHoister : public MatchFinder::MatchCallback
{
	private:
	Rewriter& R;

	public:
	StructHoister(Rewriter& Rewrite): R(Rewrite)  {}
	
	virtual void run(const MatchFinder::MatchResult &Result)
	{
		if(const RecordDecl* rd = Result.Nodes.getNodeAs<clang::RecordDecl>("rd"))
		{
         if(const LabelStmt* ls  = Result.Nodes.getNodeAs<clang::LabelStmt>("lab"))
         {
				if(rd->isCompleteDefinition() && rd->isStruct())
				{
						if(struct_list_visited.find(string(rd->getName())) == struct_list_visited.end())
						{
							struct_list_visited[string(rd->getName())] = 1;
							string line = rd->getBeginLoc().printToString(R.getSourceMgr());
							int l = get_line_no(line);
							struct_hoist_info[string(rd->getName())] = R.getRewrittenText(rd->getSourceRange());
							
							struct_hoist_info_ord[l] = R.getRewrittenText(rd->getSourceRange());
							if(struct_list.find(string(ls->getName())) == struct_list.end())
							{
									  struct_list[string(ls->getName())] = vector<string>();
							}
							stringstream ss;
							ss<<struct_hoist_info_ord[l]<<";";
							struct_list[string(ls->getName())].push_back(ss.str());
						}
				}
			}
			if(const FunctionDecl* fdecl = Result.Nodes.getNodeAs<clang::FunctionDecl>("fun"))
			{
				if(rd->isCompleteDefinition() && rd->isStruct())
				{
						if(struct_list_visited.find(string(rd->getName())) == struct_list_visited.end())
						{
							struct_list_visited[string(rd->getName())] = 1;
							string line = rd->getBeginLoc().printToString(R.getSourceMgr());
							int l = get_line_no(line);
							struct_hoist_info[string(rd->getName())] = R.getRewrittenText(rd->getSourceRange());
							
							struct_hoist_info_ord[l] = R.getRewrittenText(rd->getSourceRange());
							if(struct_list.find(string(fdecl->getName())) == struct_list.end())
							{
									  struct_list[string(fdecl->getName())] = vector<string>();
							}
							stringstream ss;
							ss<<struct_hoist_info_ord[l]<<";";
							struct_list[string(fdecl->getName())].push_back(ss.str());
						}
				}
			}
		}
	}
};


class  FunRewriter : public MatchFinder::MatchCallback
{
	private:
	Rewriter& R;

	public:
	FunRewriter(Rewriter& Rewrite): R(Rewrite)  {}
	
	virtual void run(const MatchFinder::MatchResult &Result)
	{
		if(const FunctionDecl* fdecl = Result.Nodes.getNodeAs<clang::FunctionDecl>("funlist"))
		{
				  R.RemoveText(fdecl->getBeginLoc(),orig_list[fdecl->getNameAsString()].length());
				  R.InsertText(fdecl->getBeginLoc(),updated_list[fdecl->getNameAsString()]);
				  updated_list[fdecl->getNameAsString()] = "";
		}
	}
};

class  LabRewriter : public MatchFinder::MatchCallback
{
	private:
	Rewriter& R;

	public:
	LabRewriter(Rewriter& Rewrite): R(Rewrite)  {}
	
	virtual void run(const MatchFinder::MatchResult &Result)
	{
		if(const FunctionDecl* fdecl = Result.Nodes.getNodeAs<clang::FunctionDecl>("main"))
		{
				  for(auto i:updated_list)
				  {
						R.InsertText(fdecl->getBeginLoc(),i.second);
				  }
		}
	}
};

class StructDefFinder : public MatchFinder::MatchCallback
{
	private:
	Rewriter& R;

	public:
	StructDefFinder(Rewriter& Rewrite): R(Rewrite)  {}
	
	virtual void run(const MatchFinder::MatchResult &Result)
	{
		if(const RecordDecl* rd = Result.Nodes.getNodeAs<clang::RecordDecl>("rd"))
		{
			if(const LabelStmt* ls = Result.Nodes.getNodeAs<clang::LabelStmt>("lab"))
			{
					string line = ls->getBeginLoc().printToString(R.getSourceMgr());
					int l = get_line_no(line);
		
					line = rd->getBeginLoc().printToString(R.getSourceMgr());
					int str_l = get_line_no(line);

					str_visited[str_l] = 1;

					if(str_parent.find(l) == str_parent.end())
					{
							  str_parent[l] = unordered_map<string,int>();
					}
					str_parent[l][(string(rd->getName()))] = str_l;

					/*update new name*/
					stringstream name;
					name<<renameinfo[l]<<"_"<<string(rd->getName());
					R.ReplaceText(rd->getBeginLoc().getLocWithOffset(7),name.str());
			}
			if(const FunctionDecl* fdecl = Result.Nodes.getNodeAs<clang::FunctionDecl>("fun"))
			{
					string line = fdecl->getBeginLoc().printToString(R.getSourceMgr());
					int l = get_line_no(line);
					line = rd->getBeginLoc().printToString(R.getSourceMgr());
					int str_l = get_line_no(line);

					if(str_visited.find(str_l) == str_visited.end())
					{
						if(str_parent.find(l) == str_parent.end())
						{
								  str_parent[l] = unordered_map<string,int>();
						}
						str_parent[l][string(rd->getName())] = str_l;
						/*update new name*/
						stringstream name;
						name<<renameinfo[l]<<"_"<<string(rd->getName());
						R.ReplaceText(rd->getBeginLoc().getLocWithOffset(7),name.str());
					}
			}
		}
	}
};

class StructDefRewriter : public MatchFinder::MatchCallback
{
	private:
	Rewriter& R;

	public:
	StructDefRewriter(Rewriter& Rewrite): R(Rewrite)  {}
	
	virtual void run(const MatchFinder::MatchResult &Result)
	{
		if(const VarDecl* vd = Result.Nodes.getNodeAs<clang::VarDecl>("vd"))
		{
			if(const LabelStmt* ls = Result.Nodes.getNodeAs<clang::LabelStmt>("lab"))
			{
					string line = ls->getBeginLoc().printToString(R.getSourceMgr());
					int l = get_line_no(line);
		
					line = vd->getBeginLoc().printToString(R.getSourceMgr());
					int str_l = get_line_no(line);
					string vd_name = string(vd->getName());
					string vd_type = vd->getType().getAsString();
					/*Add to str_var_visited to avoid updating next time.*/
					stringstream strvisited;
					strvisited<<vd_name<<str_l;
				//	cout<<strvisited.str()<<endl;

					str_var_visited[strvisited.str()] = 1;

					if(vd_type.find("struct ") != string::npos)
					{
						vd_type.erase(0,7);
					//	cout<<"checking for "<<vd_type<<str_l<<endl;
						stringstream ss;
						ss<<vd_name<<str_l;
						string decl_index = ss.str();

						int found = 0;

						for(auto i:depthtrav[l])
						{
								 // cout<<"checking in: "<<i<<endl;
								  if(str_parent[i].find(vd_type) != str_parent[i].end())
								  {
									//	cout<<"found in: "<<i<<endl;
										for(auto j:str_parent[i])
										{
									//			  cout<<j.first<<endl;
											if((vd_type.compare(j.first)==0) && (j.second<str_l))
											{
										      // cout<<vd_name<<"->"<<i<<endl;
							                stringstream name;
												 name<<renameinfo[i]<<"_"<<vd_type;
												 R.ReplaceText(vd->getBeginLoc().getLocWithOffset(7),name.str());

												 //update scope aswell
											//	 cout<<"updating"<<endl<<scopeinfo[l]<<endl;
												 stringstream scope_name;
												 stringstream  vd_type_ss;
												 vd_type_ss<<vd_type<<"* "<<vd_name; 
												 scope_name<<renameinfo[i]<<"_";
													
											//	 cout<<vd_name<<":searching for "<<vd_type_ss.str()<<" replacing with "<<scope_name.str()<<endl;

												 scopeinfo[l].insert(scopeinfo[l].find(vd_type_ss.str()),scope_name.str());

											//	 cout<<"updated"<<endl<<scopeinfo[l]<<endl;
												 found  = i;
								             break;
									       }
										}
									if(found!=0) break;
								  }
						}
					}
			}
			
			if(const FunctionDecl* fdecl = Result.Nodes.getNodeAs<clang::FunctionDecl>("fun"))
			{

					string line = fdecl->getBeginLoc().printToString(R.getSourceMgr());
					int l = get_line_no(line);
		
					line = vd->getBeginLoc().printToString(R.getSourceMgr());
				//	cout<<line<<endl;
					int str_l = get_line_no(line);
					string vd_name = string(vd->getName());
					string vd_type = vd->getType().getAsString();
					/*Add to str_var_visited to avoid updating next time.*/
					stringstream strvisited;
					strvisited<<vd_name<<str_l;
				//	cout<<strvisited.str()<<endl;

					if((vd_type.find("struct ") != string::npos) && (str_var_visited.find(strvisited.str()) == str_var_visited.end()))
					{
						str_var_visited[strvisited.str()] = 1;
						vd_type.erase(0,7);
					//	cout<<"checking for "<<vd_type<<str_l<<endl;
						stringstream ss;
						ss<<vd_name<<str_l;
						string decl_index = ss.str();

						int found = 0;

						for(auto i:depthtrav[l])
						{
								 // cout<<"checking in: "<<i<<endl;
								  if(str_parent[i].find(vd_type) != str_parent[i].end())
								  {
									//	cout<<"found in: "<<i<<endl;
										for(auto j:str_parent[i])
										{
									//			  cout<<j.first<<endl;
											if((vd_type.compare(j.first)==0) && (j.second<str_l))
											{
										      // cout<<vd_name<<"->"<<i<<endl;
							                stringstream name;
												 name<<renameinfo[i]<<"_"<<vd_type;
												 R.ReplaceText(vd->getBeginLoc().getLocWithOffset(7),name.str());
												 //update scope aswell
											//	 cout<<"updating"<<endl<<scopeinfo[l]<<endl;
												 stringstream scope_name;
												 stringstream  vd_type_ss;
												 vd_type_ss<<vd_type<<"* "<<vd_name; 
												 scope_name<<renameinfo[i]<<"_";
													
											//	 cout<<vd_name<<":searching for "<<vd_type_ss.str()<<" replacing with "<<scope_name.str()<<endl;

												 scopeinfo[l].insert(scopeinfo[l].find(vd_type_ss.str()),scope_name.str());

											//	 cout<<"updated"<<endl<<scopeinfo[l]<<endl;
	
												 found  = i;
								             break;
									       }
										}
									if(found!=0) break;
								  }
						}
					}
				}

			}
		}
};


class StructDefRewriter_F : public MatchFinder::MatchCallback
{
	private:
	Rewriter& R;

	public:
	StructDefRewriter_F(Rewriter& Rewrite): R(Rewrite)  {}
	
	virtual void run(const MatchFinder::MatchResult &Result)
	{
		if(const FieldDecl* vd = Result.Nodes.getNodeAs<clang::FieldDecl>("fd"))
		{
			if(const LabelStmt* ls = Result.Nodes.getNodeAs<clang::LabelStmt>("lab"))
			{
					string line = ls->getBeginLoc().printToString(R.getSourceMgr());
					int l = get_line_no(line);
		
					line = vd->getBeginLoc().printToString(R.getSourceMgr());
					int str_l = get_line_no(line);
					string vd_name = string(vd->getName());
					string vd_type = vd->getType().getAsString();
					/*Add to str_var_visited to avoid updating next time.*/
					stringstream strvisited;
					strvisited<<vd_name<<str_l;
				//	cout<<strvisited.str()<<endl;

					str_var_visited[strvisited.str()] = 1;

					if(vd_type.find("struct ") != string::npos)
					{
						vd_type.erase(0,7);
						cout<<"checking for "<<vd_type<<str_l<<endl;
						if(vd_type.find("*") != string::npos)
						{
								  vd_type.erase(vd_type.length() - 2,2);
						}
						stringstream ss;
						ss<<vd_name<<str_l;
						string decl_index = ss.str();

						int found = 0;

						for(auto i:depthtrav[l])
						{
								  cout<<"checking in: "<<i<<endl;
								  if(str_parent[i].find(vd_type) != str_parent[i].end())
								  {
										cout<<"found in: "<<i<<endl;
										for(auto j:str_parent[i])
										{
												  cout<<j.first<<endl;
											if((vd_type.compare(j.first)==0) && (j.second<str_l))
											{
										       cout<<vd_name<<"->"<<i<<endl;
							                stringstream name;
												 name<<renameinfo[i]<<"_"<<vd_type;
												 R.ReplaceText(vd->getBeginLoc().getLocWithOffset(7),name.str());
												 found  = i;
								             break;
									       }
										}
									if(found!=0) break;
								  }
						}
					}
			}
			
			if(const FunctionDecl* fdecl = Result.Nodes.getNodeAs<clang::FunctionDecl>("fun"))
			{

					string line = fdecl->getBeginLoc().printToString(R.getSourceMgr());
					int l = get_line_no(line);
		
					line = vd->getBeginLoc().printToString(R.getSourceMgr());
				//	cout<<line<<endl;
					int str_l = get_line_no(line);
					string vd_name = string(vd->getName());
					string vd_type = vd->getType().getAsString();
					/*Add to str_var_visited to avoid updating next time.*/
					stringstream strvisited;
					strvisited<<vd_name<<str_l;
				//	cout<<strvisited.str()<<endl;

					if((vd_type.find("struct ") != string::npos) && (str_var_visited.find(strvisited.str()) == str_var_visited.end()))
					{
						str_var_visited[strvisited.str()] = 1;
						vd_type.erase(0,7);

						if(vd_type.find("*") != string::npos)
						{
								  vd_type.erase(vd_type.length() - 2,2);
						}
					//	cout<<"checking for "<<vd_type<<str_l<<endl;
						stringstream ss;
						ss<<vd_name<<str_l;
						string decl_index = ss.str();

						int found = 0;

						for(auto i:depthtrav[l])
						{
								 // cout<<"checking in: "<<i<<endl;
								  if(str_parent[i].find(vd_type) != str_parent[i].end())
								  {
									//	cout<<"found in: "<<i<<endl;
										for(auto j:str_parent[i])
										{
									//			  cout<<j.first<<endl;
											if((vd_type.compare(j.first)==0) && (j.second<str_l))
											{
										      // cout<<vd_name<<"->"<<i<<endl;
							                stringstream name;
												 name<<renameinfo[i]<<"_"<<vd_type;
												 R.ReplaceText(vd->getBeginLoc().getLocWithOffset(7),name.str());
												 found  = i;
								             break;
									       }
										}
									if(found!=0) break;
								  }
						}
					}
				}

			}
		}
};



class MyASTConsumer : public ASTConsumer
{
	private:
	VarHandler VarM;
	DepthHandler DepM;
	CallResHandler CallResM;
	ExpHandler ExpM;
	CallExpRewriter CallR;
	MatchFinder Finder;
	MatchFinder Rew;
	MatchFinder Hoist;
	MatchFinder StructFinder;
	MatchFinder StructFinder2;
	MatchFinder StructFinder3;
	MatchFinder StructFinder4;
	StructInit StrR;
	ExpressionRewriter ExpR; 
	StructDefFinder StrdF;
	StructDefRewriter StrDR;
	StructDefRewriter_F StrDR_F; //for fields inside struct

	public:
	//Constructor for MyASTCOnsumer, initialize labelHandler too.
	MyASTConsumer(Rewriter& R) : VarM(R),DepM(R),CallResM(R),ExpM(R),CallR(R), StrR(R),ExpR(R), StrdF(R), StrDR(R), StrDR_F(R)
  {
	// Add things we want to search for e.g. label stmt.
	// Attach a MatchCallback to tell what action to take when that object is found.
	// also bind it so that we can take different action depending on which part of the object is found. e.g. in for loop we may want to have three different bindings one at init, one at condition check and one at increment

	 Finder.addMatcher(functionDecl(hasDescendant(labelStmt())).bind("funlist"),&DepM);
	 Finder.addMatcher(labelStmt(hasParent(compoundStmt(hasParent(functionDecl().bind("fun"))))).bind("childlabel"),&DepM);
	 Finder.addMatcher(labelStmt(hasParent(compoundStmt(hasParent(labelStmt().bind("parlabel"))))).bind("childlabel"),&DepM);

	 Finder.addMatcher(functionDecl(/*hasDescendant(labelStmt())*/).bind("funlist"),&CallResM);
	 Finder.addMatcher(labelStmt(hasParent(compoundStmt(hasParent(functionDecl().bind("fun"))))).bind("childlabel"),&CallResM);
	 Finder.addMatcher(labelStmt(hasParent(compoundStmt(hasParent(labelStmt().bind("parlabel"))))).bind("childlabel"),&CallResM);
	 Finder.addMatcher(callExpr().bind("call"),&CallResM);
	 Finder.addMatcher(callExpr(hasAncestor(labelStmt().bind("lab"))).bind("call"),&CallResM);

    Finder.addMatcher(varDecl(hasParent(declStmt(hasParent(compoundStmt(hasParent(functionDecl().bind("fun"))))))).bind("var"),&VarM); 
	 Finder.addMatcher(varDecl(hasParent(declStmt(hasParent(compoundStmt(hasParent(labelStmt().bind("label"))))))).bind("var"), &VarM);
	 Finder.addMatcher(functionDecl(hasDescendant(labelStmt())).bind("fun"),&VarM);
	 Finder.addMatcher(labelStmt().bind("lab"),&VarM);

	 Finder.addMatcher(declRefExpr(hasAncestor(labelStmt().bind("lab"))).bind("exp"), &ExpM);

	 StructFinder.addMatcher(recordDecl(hasAncestor(labelStmt().bind("lab"))).bind("rd"),&StrdF);
	 StructFinder2.addMatcher(recordDecl(hasAncestor(functionDecl().bind("fun"))).bind("rd"),&StrdF);
	 StructFinder3.addMatcher(varDecl(hasAncestor(labelStmt().bind("lab"))).bind("vd"),&StrDR);
	 StructFinder4.addMatcher(varDecl(hasAncestor(functionDecl().bind("fun")), isExpansionInMainFile()).bind("vd"),&StrDR);
	 //do same for fields
	 StructFinder3.addMatcher(fieldDecl(hasAncestor(labelStmt().bind("lab"))).bind("fd"),&StrDR_F);
	 StructFinder4.addMatcher(fieldDecl(hasAncestor(functionDecl().bind("fun")), isExpansionInMainFile()).bind("fd"),&StrDR_F);
	
   /*Mathcers to rewrite*/
	 Rew.addMatcher(callExpr().bind("call"),&CallR);
	 Rew.addMatcher(labelStmt().bind("lab"),&StrR);
	 Rew.addMatcher(declRefExpr(hasAncestor(labelStmt().bind("lab"))).bind("exp"), &ExpR);

	}

	//Called when TU is ready.
	void HandleTranslationUnit(ASTContext &Context) override
	{
		//Find all the matches in Context, specified by matchAST.
		Finder.matchAST(Context);
		StructFinder.matchAST(Context);
		StructFinder2.matchAST(Context);
		StructFinder3.matchAST(Context);
		StructFinder4.matchAST(Context);
		Rew.matchAST(Context);
		Hoist.matchAST(Context);
	}
};	

//remove child from updated list
void update_the_list()
{
		for(auto i:updated_list)
		{
				  for(auto k:lab_parent)
				  {
							 if(k.second == i.first)
							 {	
										string child = orig_list[k.first];
										updated_list[i.first].erase(updated_list[i.first].find(child),child.length());

							 }
				  }

				  for(auto f:struct_list[i.first])
				  {
							updated_list[i.first].erase(updated_list[i.first].find(f),f.length());
				  }
		}

}

//replace label names with prototypes 
void update_lab_with_proto()
{
		  for(auto i:updated_list)
		  {
					 if(i.second != "")
					 {
								stringstream ss;
								updated_list[i.first].erase(0,i.first.length()+1);
								ss<<endl<<"void "<<i.first<<"(struct s_"<<lab_parent[i.first]<<" *par_s)"<<endl<<updated_list[i.first]<<endl;
								updated_list[i.first] = ss.str();
					 }
		  }
}

class Hoist_ASTConsumer : public ASTConsumer
{
	private:
	MatchFinder Finder;
	Hoist_out HstR;
	MatchFinder Rew;
	MatchFinder LabRew;
	MatchFinder StructRew;
	MatchFinder StructRew1;
	FunRewriter FunR;
	LabRewriter LabR;
	StructHoister StructR;
//	VarHandler_Str VarM;

	public:
	//Constructor for MyASTCOnsumer, initialize labelHandler too.
	Hoist_ASTConsumer(Rewriter& R) : HstR(R),FunR(R),LabR(R),StructR(R)//, VarM(R)
   {
			  Finder.addMatcher(functionDecl(hasDescendant(labelStmt())).bind("funlist"),&HstR);
			  Finder.addMatcher(labelStmt(hasParent(compoundStmt(hasParent(functionDecl().bind("fun"))))).bind("childlabel"),&HstR);
			  Finder.addMatcher(labelStmt(hasParent(compoundStmt(hasParent(labelStmt().bind("parlabel"))))).bind("childlabel"),&HstR);

		//     Finder.addMatcher(varDecl(hasParent(declStmt(hasParent(compoundStmt(hasParent(functionDecl().bind("fun"))))))).bind("var"),&VarM); 
		 //	  Finder.addMatcher(varDecl(hasParent(declStmt(hasParent(compoundStmt(hasParent(labelStmt().bind("label"))))))).bind("var"), &VarM);
		 //	  Finder.addMatcher(functionDecl(hasDescendant(labelStmt())).bind("fun"),&VarM);
		 //	  Finder.addMatcher(labelStmt().bind("lab"),&VarM);

			  StructRew.addMatcher(recordDecl(hasAncestor(labelStmt().bind("lab")),isExpansionInMainFile()).bind("rd"),&StructR);
			  StructRew1.addMatcher(recordDecl(hasAncestor(functionDecl().bind("fun")),isExpansionInMainFile()).bind("rd"),&StructR);
			  Rew.addMatcher(functionDecl(hasDescendant(labelStmt())).bind("funlist"),&FunR);
			  LabRew.addMatcher(functionDecl(isMain()).bind("main"),&LabR);

	}

	//Called when TU is ready.
	void HandleTranslationUnit(ASTContext &Context) override
	{
		//Find all the matches in Context, specified by matchAST.
		Finder.matchAST(Context);
		StructRew.matchAST(Context);
		StructRew1.matchAST(Context);
		update_the_list();
		Rew.matchAST(Context);
		update_lab_with_proto();
		LabRew.matchAST(Context);
	}
};	

class MyFrontendAction : public ASTFrontendAction
{
	private:
	Rewriter TheRewriter;

	public:
	MyFrontendAction() {}

//At the end of sourcefile write to source.
	void EndSourceFileAction() override
	{

   	//Emit the rewritten buffer to file rewrite_output.c to be used for hoisting.
		//ofstream outfile("rewrite_output.c");
		error_code error_code;
		llvm::raw_fd_ostream outFile("rewriter_op.c",error_code, llvm::sys::fs::F_None);
      TheRewriter.getEditBuffer(TheRewriter.getSourceMgr().getMainFileID()).write(outFile);
	   outFile.close();	
		

		/*Run AST for newfile*/
		//hoister_fun();	

	/*	cout<<"Struct decl"<<endl;
		for(auto i:str_parent)
		{
			cout<<i.first;
			for(auto j:i.second)
			{
				cout<<"->"<<j.first<<"("<<j.second<<")";
			}
			cout<<endl;
		}			
		
      cout << "\nVarinfo : \n"; 
      for (auto itr: varinfo) 
      { 
        cout << itr.first << endl;
			for(auto i:itr.second)
			{
				cout<<i.second<<" "<<i.first<<endl;
			}
      }

		cout<<endl<<"Arrinfo : "<<endl;
		for(auto i:arrinfo)
		{
				  cout<<i.first<<":";
//				  for(int j = (i.second.length()-1);j>=0;j--)
					for(auto j:i.second)
				  {
							cout<<"["<<j<<"]";
				  }
				  cout<<endl;
		}
		//scopeinfo verifier
		cout<<endl<<"Scopes :"<<endl;
	//	map<int,string> scopeinfo_ord(scopeinfo.begin(), scopeinfo.end());
		for(auto i: scopeinfo_ord)
		{
			cout<<i.first<<endl<<i.second<<endl;
		}

   	cout<<"scope inits:"<<endl;
		map<int,string> scopeinit_ord(scopeinit.begin(), scopeinit.end());
		for(auto i: scopeinit_ord)
		{
			cout<<i.first<<endl<<i.second<<endl;
		}

		for(auto i:prototypes_ord)
		{
			cout<<i.second<<";"<<endl;
		}			

		for(auto i:renameinfo)
		{
			cout<<i.first<<" "<<i.second<<" "<<endl;
		}

		cout<<"Depth of labels and functons:"<<endl;
		for(auto i:depthinfo)
		{
			cout<<i.first<<" "<<i.second<<" "<<endl;
		}

		for(auto i:nameinfo)
		{
			cout<<i.first<<" "<<i.second<<" "<<endl;
		}

		for(auto i:depthtrav)
		{
			cout<<i.first;
			for(auto j:i.second)
			{
				cout<<"->"<<j;
			}
			cout<<endl;
		}

		for(auto i:rangeinfo)
		{
			cout<<i.first<<" "<<i.second<<endl;
		}

		for(auto i:callresinfo)
		{
			cout<<i.first<<" ";

			for(auto j:i.second)
			{
				cout<<"->"<<"("<<j.first<<","<<j.second<<")";
			}
			cout<<endl;
		}

		for(auto i:callres)
		{
			cout<<i.first<<"->"<<i.second<<endl;
		}


		cout<<"Declarations depths: "<<endl;
		for(auto i: vardepth)
		{
			cout<<i.first<<" "<<i.second<<endl;
		}

		cout<<"use distance from their declarations: "<<endl;
		for(auto i:refdepth)
		{
			cout<<i.first<<" "<<i.second<<endl;
		}
 
		cout<<"global fun list:"<<endl;
		for(auto i:globalfun)
		{
				  cout<<i.first<<"   "<<i.second<<endl;
		}

		cout<<"Callexpression distance from their definition:"<<endl;
		for(auto i:callexprdepth)
		{
			cout<<i.first<<"-->"<<i.second<<endl;
		}
*/	}


	virtual std::unique_ptr<clang::ASTConsumer> CreateASTConsumer(clang::CompilerInstance &CI, llvm::StringRef InFile)
	{
		TheRewriter.setSourceMgr(CI.getSourceManager(), CI.getLangOpts());
		//Pass rewriter object to constructor of MyASTConsumer
		return std::make_unique<MyASTConsumer>(TheRewriter);
	}
};


class Hoist_FrontendAction : public ASTFrontendAction
{
	private:
	Rewriter TheRewriter;

	public:
	Hoist_FrontendAction() {}

//At the end of sourcefile write to source.
	void EndSourceFileAction() override
	{
		//write all structures and prototypes to labels.h

		ofstream headerfile("labels.h");
		for(auto i:struct_hoist_info_ord)
		{
			headerfile<<i.second<<";"<<endl;
		}

		map<int,string> scopeinfo_ord(scopeinfo.begin(), scopeinfo.end());
		for(auto i: scopeinfo_ord)
		{
			headerfile<<i.second<<endl;
		}

		headerfile<<endl;

		map<int,string> prototypes_ord(prototypes.begin(), prototypes.end());
		for(auto i:prototypes_ord)
		{
			headerfile<<i.second<<";"<<endl;
		}

		headerfile.close();			

 
		TheRewriter.getEditBuffer(TheRewriter.getSourceMgr().getMainFileID()).write(llvm::outs());

/*		cout<<"label-parent list:"<<endl;
		for(auto i:lab_parent)
		{
			cout<<i.first<<"   "<<i.second<<endl;
		}

		cout<<"original list:"<<endl;
		for(auto i:orig_list)
		{
				  cout<<i.first<<":"<<endl<<i.second<<endl;
		}
*/
		cout<<"updated_list:"<<endl;
		for(auto i:updated_list)
		{
				  cout<<i.first<<":"<<endl<<i.second<<endl;
		}

		cout<<"struct list:"<<endl;
		for(auto i:struct_list)
		{
				  cout<<i.first<<":"<<endl;
				  for(auto j:i.second)
				  {
							 cout<<j<<endl;
				  }
		}

	}


	virtual std::unique_ptr<clang::ASTConsumer> CreateASTConsumer(clang::CompilerInstance &CI, llvm::StringRef InFile)
	{
		TheRewriter.setSourceMgr(CI.getSourceManager(), CI.getLangOpts());
		//Pass rewriter object to constructor of MyASTConsumer
		return std::make_unique<Hoist_ASTConsumer>(TheRewriter);
	}
};

/*Pass 1*/
int main(int argc, const char **argv) {

		  //Reads the command line argument and interprets it, also loads the compilation database.
		  {
					 CommonOptionsParser OptionsParser(argc, argv, MyToolCategory);

					 //Tool (Reference to loaded compilations database. , List of files to process )
					 ClangTool Tool(OptionsParser.getCompilations(), OptionsParser.getSourcePathList());

					 //Runs an action over all files specified in the command line. 

					 //Takes "action to be performed" as input.
					 // Here we are providing FrontendAction, i.e. run FrontendAction on all the files.
					 //newFrontendActionFactory returns unique_ptr of type MyFrontendAction. 
					 Tool.run(newFrontendActionFactory<MyFrontendAction>().get());
		  }
		  hoister_fun();

		  return 0;
}

/*Pass 2*/
void hoister_fun() {
  int	argc = 2;
  const char* argv[2];
  argv[1] = "rewriter_op.c";
  argv[0] = "bin/loop-convert";
  //Reads the command line argument and interprets it, also loads the compilation database.
  CommonOptionsParser OptionsParser(argc, argv, MyToolCategory1);

  std::vector<std::string> sourcepathlist;
  sourcepathlist.push_back("rewriter_op.c");
  //Tool (Reference to loaded compilations database. , List of files to process )
  ClangTool Hoist_Tool(OptionsParser.getCompilations(),sourcepathlist);// OptionsParser.getSourcePathList());


	cout<<"#include\"labels.h\""<<endl;
  
  //Runs an action over all files specified in the command line. 

  //Takes "action to be performed" as input.
  // Here we are providing FrontendAction, i.e. run FrontendAction on all the files.
  //newFrontendActionFactory returns unique_ptr of type MyFrontendAction. 
  Hoist_Tool.run(newFrontendActionFactory<Hoist_FrontendAction>().get());
}
