/**
    Tyler Lofgren
    CS 3100
    HW5: Shell with redirection
    Last updated 2/24/15

    simpleShell.cpp
*/

#include <iostream>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <string>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <chrono>
#include <vector>
#include <regex>

/** Exe
 *  A simple struct for individual executables in a command.
 */
struct Exe
{
    std::string name;           //name of executable
    std::vector<char*> args;    //arguments to executable
    int infd;                   //in file descriptor
    int outfd;                  //out file descriptor
    Exe(std::string n, std::vector<char*> a)
    {
        name = n;
        args = a;
        infd = STDIN_FILENO;    //initialize to stdin
        outfd = STDOUT_FILENO;  //initialize to stdout
    }
};

/** ptime
 *  Calculates time in seconds, milliseconds, microseconds from std::chrono::duration dur
 *  and prints to stdout
 */
void ptime(std::chrono::duration<double> dur)
{
    using namespace std::chrono;

    unsigned long us = static_cast<unsigned long>(duration_cast<microseconds>(dur).count());
    auto ms = us / 1000;
    us %= 1000;
    auto sec = ms / 1000;
    ms %= 1000;
    std::cout << "Time spent executing child processes: " << sec << " seconds, "
                           << ms << " milliseconds, and "
                           << us << " microseconds" << std::endl;
}

/** parseLine
 *  Parses the string line into vector v of char arrays using delimiter delim.
 *  Used in parseCommand function.
 *
 *  Warning: Since this is designed to be used in a call to execvp, the last
 *  element of the vector returned is a null pointer.
 */
std::vector<char*> parseLine(const char* line, const char* delim)
{
    std::vector<char*> v;
    char* nonconst = nullptr;
    nonconst = strdup(line);
    char* firstToken = strtok(nonconst, delim);
    if (firstToken != nullptr)
    {
        v.push_back(firstToken);
        while (1)
        {
            char* nextToken = nullptr;
            nextToken = strtok(nullptr, delim);
            v.push_back(nextToken);
            if (nextToken == nullptr)
                break;
        }
    }
    return v;
}

/** parseCommand
 *  Parses a given command line cmd into a vector of Exe objects.
 *
 *  cmd: Full string input to shell.
 *  inFileName: file name from which the shell will redirect input.
 *              Will be empty if user did not redirect input with '<'.
 *  outFileName: file name to which the shell will redirect input.
  *              Will be empty if user did not redirect output with '>'.
 */
std::vector<Exe> parseCommand(std::string& cmd, std::string& inFileName, std::string& outFileName)
{
    /* Algorithm
    Find '<'
        Find next token and put into inFileName
        Extract < token from cmd string
    Find '>'
        Find next token and put into outFileName
        Extract > token from cmd string
    while not end of string
        Find next pipe
        Update cmdStart and cmdEnd
    */
    
    std::vector<Exe> execs;

    //find and extract <
    auto loc = cmd.find("<", 0);
    if (loc != std::string::npos)
    {
        auto tokenStart = loc + 1;     //account for "< token" (space in between)
        auto tokenEnd = cmd.find(" ", tokenStart);
        while (tokenEnd - tokenStart == 0)
        {
            tokenStart++;
            tokenEnd = cmd.find(" ", tokenStart);
        }
        if (tokenEnd == std::string::npos)
            tokenEnd = cmd.size();
        inFileName = cmd.substr(tokenStart, tokenEnd - tokenStart);
        cmd.erase(cmd.begin() + loc - 1, cmd.begin() + tokenEnd);
    }

    //find and extract >
    loc = cmd.find(">", 0);
    if (loc != std::string::npos)
    {
        auto tokenStart = loc + 2;     //account for "< token" (space in between)
        auto tokenEnd = cmd.find(" ", tokenStart);
        if (tokenEnd == std::string::npos)
            tokenEnd = cmd.size();
        outFileName = cmd.substr(tokenStart, tokenEnd - tokenStart);
        cmd.erase(cmd.begin() + loc - 1, cmd.begin() + tokenEnd);
    }
    
    //find |
    auto cmdStart = 0;
    auto cmdEnd = cmd.find("|",cmdStart);
    std::vector<char*> args;
    while (cmdEnd >= 0 && cmdStart < cmd.size())
    {
        args.clear();
        if (cmdEnd == std::string::npos)
        {
            cmdEnd = cmd.size();
        }
        std::string line = cmd.substr(cmdStart, cmdEnd - cmdStart);
        args = parseLine(line.c_str(), " ");

        //construct exe
        Exe e(args[0], args);
        execs.push_back(e);
        cmdStart = cmdEnd + 1;
        cmdEnd = cmd.find("|", cmdStart);
    }

    return execs;
}

/**runExe
 * Takes an Exe object and executes it
 * Redirects I/O according to object's file descriptor members
 * Used ideas from http://en.wikibooks.org/wiki/Operating_System_Fundamentals
 */
int runExe(Exe& ex, std::chrono::duration<double>& childrenTime)
{
    auto start = std::chrono::high_resolution_clock::now();     //begin timer of child process
    auto pid = fork();
    if (pid == 0)       //This is the child
    {
        if (ex.infd != STDIN_FILENO)
        {
            if (dup2(ex.infd, STDIN_FILENO) < 0)
            {
                std::cerr << "Error redirecting input to '" << ex.name << "'!\n";
                return -1;
            }
        }
        if (ex.outfd != STDOUT_FILENO)
        {
            if (dup2(ex.outfd, STDOUT_FILENO) < 0)
            {
                std::cerr << "Error redirecting output to '" << ex.name << "'!\n";
                return -1;
            }
        }
        execvp(ex.args[0], ex.args.data());
        exit(1);
        std::cerr << "ERROR: IF YOU SEE THIS, CHILD WASN'T DESTROYED" << std::endl;
    }
    else if (pid > 0)   //Parent
    {
        int status;
        if (waitpid(pid, &status, 0) == -1)     //error with waitpid
            std::cerr << "Error: There was an error calling waitpid" << std::endl;
        if (WIFEXITED(status) && WEXITSTATUS(status) != 0)	//error during execution
        {
                std::cerr << "Uh-oh: Child process returned with value " << WEXITSTATUS(status)
                          << " while trying to execute '" << ex.name << "'" << std::endl;
        }
        auto end = std::chrono::high_resolution_clock::now();
        childrenTime += (end - start);
    }
    else    //pid < 0
    {
        std::cerr << "Error: Could not fork!" << std::endl;
        return -2;
    }
    return 0;
}

int main(void)
{
    std::string cmd = "";                       //user's line of input
    std::string prompt = "[cmd]: ";             //prompt for commands
    std::string firstInFile;                    //redirect from inputfile to first executable
    std::string finalOutFile;                   //redirect last output to file
    std::chrono::duration<double> childrenTime; //amount of time spent executing child processes
    std::vector<std::string> cmdHistory;        //maintain history of commands
    std::vector<Exe> exes;                      //vector of Exe objects to execute
    std::regex caret_regex("\\^\\d+");          //user can execute a past command with ^X, x= some number
    std::cmatch regex_result;                   //store results of matching to regex

    while (cmd != "exit")
    {
        //initialize
        cmd = "";
        firstInFile = "";
        finalOutFile = "";

        //prompt user
        std::cout << prompt;
        std::getline(std::cin, cmd);
        while (cmd == "")       //user just hit enter
        {
            std::cout << prompt;
            std::getline(std::cin, cmd);
        }
        cmdHistory.push_back(cmd);      //store in history for user

        //check for history command
        if (std::regex_match(cmd.c_str(), regex_result, caret_regex))
        {
            auto spaceLoc = cmd.find(" ");
            if (spaceLoc == std::string::npos)
                spaceLoc = cmd.size();
            // std::cout << "regex matched to " << regex_result.str(0) << std::endl;    //debug
            const char* match = regex_result.str(0).substr(1, spaceLoc - 1).c_str();	//get number from match
            int steps = atoi(match);
            if (steps < cmdHistory.size())	//check for out of bounds
            {
                cmd = cmdHistory[cmdHistory.size() - 1 - steps];
                cmdHistory.pop_back();
                cmdHistory.push_back(cmd);
                std::cout << "Executing command " << steps << " steps back: '" << cmd << "'" << std::endl;
            }
            else	//too big: notify user
            {
                std::cerr << "Command requested was farther back than history allows. Farthest step back is "
                          << (cmdHistory.size() - 1) << std::endl;
                continue;
            }
        }

        exes = parseCommand(cmd, firstInFile, finalOutFile);

        int pipes[exes.size()-1][2];        //allow for a pipe between each exe
        int currPipe = 0;                   //keep track of current exe's I/O redirection
        int prevPipe;                       //keep track of previous exe's I/O
        int firstIndex = 0;                 //index of first element in exes
        int lastIndex = exes.size() - 1;    //index of last element in exes
        for (auto i = 0; i < exes.size(); i++)
        {
            //check for custom shell commands
            if (strcmp(exes[i].name.c_str(), "ptime") == 0)
            {
                auto start = std::chrono::high_resolution_clock::now();
                ptime(childrenTime);
                auto end = std::chrono::high_resolution_clock::now();
                childrenTime += (end - start);
            }
            else if (strcmp(exes[i].name.c_str(), "exit") == 0)
            {
                cmd = "exit";
                break;
            }
            else	//create new process and execute
            {
                if (i == firstIndex && !firstInFile.empty())       //redirect input to be from file
                {
                    int fd = open(firstInFile.c_str(), O_RDONLY);
                    if (fd >= 0)
                    {
                        exes[i].infd = fd;
                    }
                    else
                    {
                        std::cerr << "Error reading '" << firstInFile << "'!" << std::endl;
                        break;      //exit for loop
                    }
                }
                if (!finalOutFile.empty() && i == lastIndex)      //redirect output of last exe to a file
                {
                    int fd = open(finalOutFile.c_str(), O_WRONLY | O_CREAT, S_IRWXU);
                                        //if file does not exist, create with rwx permissions
                    if (fd >= 0)
                    {
                        exes[i].outfd = fd;
                    }
                    else
                    {
                        std::cerr << "Error writing to '" << finalOutFile << "'!" << std::endl;
                        break;      //exit for loop
                    }
                }
                

                if (exes.size() > 1)            //multiple exes with pipes
                {
                    if (i != firstIndex)
                    {
                        exes[i].infd = pipes[prevPipe][0];
                    }
                    if (i != lastIndex)
                    {
                        if (pipe(pipes[currPipe]) != 0)
                        {
                            std::cerr << "Error occured while creating pipe!" << std::endl;
                            break;  //break out of for loop
                        }
                        exes[i].outfd = pipes[currPipe][1];
                    }
                }
                
                //execute
                runExe(exes[i], childrenTime);

                if (exes.size() > 1)
                {
                    close(pipes[currPipe][1]);      //close the writing end of this pipe
                    if (i != firstIndex)
                        close(pipes[prevPipe][0]);  //close the reading end of previous pipe
                }
                prevPipe = currPipe;
                currPipe++;
            }//end else
        }//end for
    }//end while

    return 0;
}
