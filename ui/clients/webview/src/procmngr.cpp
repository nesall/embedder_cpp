//#include "procmngr.h"
//#include <iostream>
//#include <sstream>
//
//ProcessManager::ProcessManager() : running(false), exitCode(-1) {
//#ifdef _WIN32
//    ZeroMemory(&processInfo, sizeof(processInfo));
//    
//    // Create a job object to manage child processes
//    jobObject = CreateJobObject(NULL, NULL);
//    if (jobObject) {
//        JOBOBJECT_EXTENDED_LIMIT_INFORMATION jeli = { 0 };
//        jeli.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
//        SetInformationJobObject(jobObject, JobObjectExtendedLimitInformation, &jeli, sizeof(jeli));
//    }
//#endif
//}
//
//ProcessManager::~ProcessManager() {
//    stopProcess();
//    
//#ifdef _WIN32
//    if (jobObject) {
//        CloseHandle(jobObject);
//    }
//#endif
//}
//
//bool ProcessManager::startProcess(const std::string& command, const std::vector<std::string>& args) {
//    if (running) {
//        std::cerr << "Process is already running" << std::endl;
//        return false;
//    }
//
//#ifdef _WIN32
//    // Build command line
//    std::string cmdLine = command;
//    for (const auto& arg : args) {
//        cmdLine += " " + arg;
//    }
//
//    STARTUPINFOA startupInfo;
//    ZeroMemory(&startupInfo, sizeof(startupInfo));
//    startupInfo.cb = sizeof(startupInfo);
//    ZeroMemory(&processInfo, sizeof(processInfo));
//
//    // Create process
//    BOOL success = CreateProcessA(
//        NULL,                   // No module name (use command line)
//        const_cast<LPSTR>(cmdLine.c_str()), // Command line
//        NULL,                   // Process handle not inheritable
//        NULL,                   // Thread handle not inheritable
//        FALSE,                  // Set handle inheritance to FALSE
//        0,                      // No creation flags
//        NULL,                   // Use parent's environment block
//        NULL,                   // Use parent's starting directory
//        &startupInfo,           // Pointer to STARTUPINFO structure
//        &processInfo            // Pointer to PROCESS_INFORMATION structure
//    );
//
//    if (!success) {
//        std::cerr << "Failed to create process. Error: " << GetLastError() << std::endl;
//        return false;
//    }
//
//    // Add process to job object for proper cleanup
//    if (jobObject) {
//        AssignProcessToJobObject(jobObject, processInfo.hProcess);
//    }
//
//    running = true;
//    return true;
//
//#else
//    // Unix/Linux implementation
//    pid = fork();
//    
//    if (pid == -1) {
//        std::cerr << "Failed to fork process" << std::endl;
//        return false;
//    }
//    
//    if (pid == 0) {
//        // Child process
//        std::vector<char*> argv;
//        argv.push_back(const_cast<char*>(command.c_str()));
//        
//        for (const auto& arg : args) {
//            argv.push_back(const_cast<char*>(arg.c_str()));
//        }
//        argv.push_back(nullptr); // NULL terminated
//        
//        execvp(command.c_str(), argv.data());
//        
//        // If execvp returns, there was an error
//        std::cerr << "Failed to execute command: " << command << std::endl;
//        exit(EXIT_FAILURE);
//    } else {
//        // Parent process
//        running = true;
//        return true;
//    }
//#endif
//}
//
//bool ProcessManager::stopProcess() {
//    if (!running) {
//        return true;
//    }
//
//#ifdef _WIN32
//    if (processInfo.hProcess) {
//        // Terminate the process
//        if (TerminateProcess(processInfo.hProcess, 1)) {
//            WaitForSingleObject(processInfo.hProcess, INFINITE);
//            CloseHandle(processInfo.hProcess);
//            CloseHandle(processInfo.hThread);
//            ZeroMemory(&processInfo, sizeof(processInfo));
//            running = false;
//            exitCode = 1;
//            return true;
//        }
//    }
//#else
//    if (kill(pid, SIGTERM) == 0) {
//        // Wait for process to terminate
//        int status;
//        waitpid(pid, &status, 0);
//        running = false;
//        
//        if (WIFEXITED(status)) {
//            exitCode = WEXITSTATUS(status);
//        } else {
//            exitCode = -1;
//        }
//        return true;
//    }
//#endif
//
//    return false;
//}
//
//bool ProcessManager::isRunning() const {
//    if (!running) {
//        return false;
//    }
//
//#ifdef _WIN32
//    DWORD exitCode;
//    if (GetExitCodeProcess(processInfo.hProcess, &exitCode)) {
//        if (exitCode == STILL_ACTIVE) {
//            return true;
//        } else {
//            // Process has exited
//            const_cast<ProcessManager*>(this)->running = false;
//            const_cast<ProcessManager*>(this)->exitCode = exitCode;
//            return false;
//        }
//    }
//    return false;
//#else
//    int status;
//    pid_t result = waitpid(pid, &status, WNOHANG);
//    
//    if (result == 0) {
//        // Process is still running
//        return true;
//    } else if (result == -1) {
//        // Error
//        const_cast<ProcessManager*>(this)->running = false;
//        return false;
//    } else {
//        // Process has exited
//        const_cast<ProcessManager*>(this)->running = false;
//        if (WIFEXITED(status)) {
//            const_cast<ProcessManager*>(this)->exitCode = WEXITSTATUS(status);
//        } else {
//            const_cast<ProcessManager*>(this)->exitCode = -1;
//        }
//        return false;
//    }
//#endif
//}
//
//bool ProcessManager::waitForCompletion(int timeoutMs) {
//    if (!running) {
//        return true;
//    }
//
//#ifdef _WIN32
//    DWORD result = WaitForSingleObject(processInfo.hProcess, 
//                                      timeoutMs == -1 ? INFINITE : timeoutMs);
//    
//    if (result == WAIT_OBJECT_0) {
//        DWORD exitCode;
//        if (GetExitCodeProcess(processInfo.hProcess, &exitCode)) {
//            this->exitCode = exitCode;
//        }
//        running = false;
//        CloseHandle(processInfo.hProcess);
//        CloseHandle(processInfo.hThread);
//        ZeroMemory(&processInfo, sizeof(processInfo));
//        return true;
//    }
//    
//    return false;
//#else
//    int status;
//    if (timeoutMs == -1) {
//        // Wait indefinitely
//        pid_t result = waitpid(pid, &status, 0);
//        if (result != -1) {
//            running = false;
//            if (WIFEXITED(status)) {
//                exitCode = WEXITSTATUS(status);
//            }
//            return true;
//        }
//    } else {
//        // Implement timeout (simplified - in production you'd use select/poll)
//        for (int i = 0; i < timeoutMs / 100; ++i) {
//            if (!isRunning()) {
//                return true;
//            }
//#ifdef _WIN32
//            Sleep(100);
//#else
//            usleep(100000); // 100ms
//#endif
//        }
//    }
//    return false;
//#endif
//}
//
//int ProcessManager::getExitCode() const {
//    return exitCode;
//}
//
//uint64_t ProcessManager::getProcessId() const
//{
//#ifdef _WIN32
//  return static_cast<uint64_t>(processInfo.dwProcessId);
//#else
//  return static_cast<uint64_t>(pid);
//#endif
//}
