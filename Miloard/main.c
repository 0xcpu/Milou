#include "Miloard.h"


// ASCII art headers, help message
LPCSTR MILOU_LOGO_HEADER =
" _______________________________________________________________________________ \n"
"|                              %.                                               |\n"
"|                            %%.   ./#%%                                        |\n"
"|             ,      ,%%%.  #          %(                                       |\n"
"|              %&%%*     %    *%%%%    %/.                                      |\n"
"|            &%*.*%        %.    *.%    %                                       |\n"
"|                   %      .%%%/%    .,(%%                                      |\n"
"|             (&&%* * %     / %       #   #                                     |\n"
"|                      %.*    /            %                                    |\n"
"|                      ,#                   %                                   |\n"
"|                      %       %%       *%  ../(/                               |\n"
"|                      %       .            ..  %%                              |\n"
"|                     ,%%              /%%(%%%%%* %                             |\n"
"|                      %              /%* %%%%%%%  %                            |\n"
"|                      (%     ,        %%%%%%%%%   %                            |\n"
"|                      %/    (                    /%                            |\n"
"|                      %.    % #                 (#                             |\n"
"|                      %         (,              %                              |\n"
"|                    *%(%%#    %   %%/(# ( *%*/%/      %%%%%#                   |\n"
"|                  *       ,%# #/  %% .% /% %       #%        %,                |\n"
"|               %.           %/ %   %  , *  /     %#           *%               |\n"
"|*************%%,,*%          %**%   %%%%  *****%***%%           %#(((((((((((##|\n"
"|                  &%           %.%.,     (#  ,      /             %            |\n"
"|                   %            %     , .           %%   *(   %   (            |\n"
"|                   *    %   %   %                   .*    #.  %  *%            |\n"
"|                   .%& (%   %%%%                      %*#%%,%%%%%              |\n"
"|_______________________________________________________________________________|\n";

LPCSTR MILOU_HELP =
">                     Miloard - Milou Loader Version 0.1                        <\n"
"Usage:                                                                           \n"
"        load <driver path> - load Milou                                          \n"
"        unload             - unload Milou                                      \n\n"
">                                  0xcpu 2019                                   <\n";

#define MILOU_ACTION_LOAD   "load"
#define MILOU_ACTION_UNLOAD "unload"
#define MILOU_SERVICE_NAMEW L"MilouSvc"


int main(int argc, char **argv)
{
    DWORD   retCode = EXIT_SUCCESS;
    DWORD   dwDrvPathLenReq;
    DWORD   dwDrvPathLen;
    SIZE_T  drvNameLen;
    PWSTR   pwDrvName = NULL;
    PWSTR   pwDrvPath = NULL;

    if (argc > 1) {
        if (_strnicmp(argv[1], MILOU_ACTION_LOAD, strlen(MILOU_ACTION_LOAD)) == 0) {
            if (argc < 3) {
                goto help;
            }
            
            puts("Loading Milou...");

            drvNameLen = strlen(argv[2]) + 1;
            pwDrvName = calloc(drvNameLen, sizeof(WCHAR));
            if (NULL == pwDrvName) {
                retCode = GetLastError();
                fprintf(stderr, "[Miloard] Failed allocating memory for driver name: %#X\n", retCode);
            } else {
                if (MultiByteToWideChar(CP_OEMCP,
                                        MB_ERR_INVALID_CHARS,
                                        argv[2],
                                        -1,
                                        pwDrvName,
                                        (INT)drvNameLen) == 0) {
                    retCode = GetLastError();
                    fprintf(stderr, "[Miloard] Failed initializing driver name: %#X\n", retCode);
                } else {
                    dwDrvPathLenReq = GetFullPathNameW(pwDrvName, 0, NULL, NULL) + 1;
                    pwDrvPath = calloc(dwDrvPathLenReq, sizeof(WCHAR));
                    if (NULL == pwDrvPath) {
                        retCode = GetLastError();
                        fprintf(stderr, "[Miloard] Failed allocating memory for driver path: %#X\n", retCode);
                    } else {
                        dwDrvPathLen = GetFullPathNameW(pwDrvName, dwDrvPathLenReq, pwDrvPath, 0);
                        if ((dwDrvPathLen > 0) && (dwDrvPathLen < dwDrvPathLenReq)) {
                            // no checks on driver path validity
                            MiloardManageDriver(pwDrvPath, MILOU_SERVICE_NAMEW, MILOARD_ACTION_INSTALL);
                        } else {    
                            retCode = GetLastError();
                            fprintf(stderr,
                                    "[Miloard] Failed retrieving absolute driver path: %#X retrieved: %#X/%#X bytes\n",
                                    retCode,
                                    dwDrvPathLen,
                                    dwDrvPathLenReq);
                        }

                        free(pwDrvPath);
                        pwDrvPath = NULL;
                    }                    
                }
                
                free(pwDrvName);
                pwDrvName = NULL;
            }
        } else if (_strnicmp(argv[1], MILOU_ACTION_UNLOAD, strlen(MILOU_ACTION_UNLOAD)) == 0) {
            puts("Unloading Milou...");

            MiloardManageDriver(L"test", MILOU_SERVICE_NAMEW, MILOARD_ACTION_UNINSTALL);
        } else {
            goto help;
        }
    } else {
        help:
        printf("%s\n", MILOU_LOGO_HEADER);
        printf("%s\n", MILOU_HELP);
    }

    return EXIT_SUCCESS;
}