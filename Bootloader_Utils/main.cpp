#include <stdio.h>
#include <stdlib.h>

#include  <Windows.h>
#include "cybtldr_api.h"
#include "cybtldr_api2.h"

HANDLE hComm;

static int OpenConnection(void)
{
	return 0;
}

static int CloseConnection(void)
{
	return 0;
}

void error_display(void)
{
	LPVOID lpMsgBuf;
	FormatMessage(
	    FORMAT_MESSAGE_ALLOCATE_BUFFER |
	    FORMAT_MESSAGE_FROM_SYSTEM |
	    FORMAT_MESSAGE_IGNORE_INSERTS,
	    NULL,
	    GetLastError(),
	    MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
	    (LPTSTR)&lpMsgBuf,
	    0,
	    NULL
	);
	// Process any inserts in lpMsgBuf.
	// ...
	// Display the string.
	MessageBox(NULL, (LPCTSTR)lpMsgBuf, L"Error", MB_OK | MB_ICONINFORMATION);
	// Free the buffer.
	LocalFree(lpMsgBuf);
}

static int ReadData(unsigned char *data, int count)
{
	DWORD dwRead = 0;
	BOOL fWaitingOnRead = FALSE;
	OVERLAPPED osReader = { 0 };

	// Create the overlapped event. Must be closed before exiting
	// to avoid a handle leak.
	osReader.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

	if (osReader.hEvent == NULL) {
		// Error creating overlapped event; abort.
		return 1;
	}
	memset(data, 0, count);

	// insert a delay
	Sleep( 100 );

	if (!fWaitingOnRead) {
		// Issue read operation.
		if (!ReadFile(hComm, data, count, &dwRead, &osReader)) {
			if (GetLastError() != ERROR_IO_PENDING) {    // read not delayed?
				// Error in communications; report it.
			} else {
				fWaitingOnRead = TRUE;
			}
		} else {
			// read completed immediately
			return 0;

		}
	}

	return 1;
}

static int ReadDataX(unsigned char *data, int count)
{
	DWORD dwCommEvent;
	DWORD dwRead;
	char  chRead;
	unsigned int index = 0;

	if (!SetCommMask(hComm, EV_RXCHAR)) {
		// Error setting communications event mask
		return 1;
	}

	memset(data, 0, count);


	for (;;) {
		if (WaitCommEvent(hComm, &dwCommEvent, NULL)) {
			do {
				if (ReadFile(hComm, &chRead, 1, &dwRead, NULL)) {
					// A byte has been read; process it.
					data[index++] = chRead;

					if (index == count)
						return 0;
				} else {
					error_display();
					printf("GetLastError %d\n", GetLastError());
					return 1;
					// An error occurred in the ReadFile call.
				}
				break;
			} while (dwRead);
		} else
			// Error in WaitCommEvent
			return 1;
		break;

	}

	return 0;

}

BOOL WriteABuffer(unsigned char * lpBuf, DWORD dwToWrite)
{
	OVERLAPPED osWrite = { 0 };
	DWORD dwWritten;
	DWORD dwRes;
	BOOL fRes;

	// Create this write operation's OVERLAPPED structure's hEvent.
	osWrite.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (osWrite.hEvent == NULL) {
		// error creating overlapped event handle
		return FALSE;
	}

	// Issue write.
	if (!WriteFile(hComm, &lpBuf[1], dwToWrite, &dwWritten, &osWrite)) {
		if (GetLastError() != ERROR_IO_PENDING) {
			// WriteFile failed, but isn't delayed. Report error and abort.
			fRes = FALSE;
		} else
			// Write is pending.
			dwRes = WaitForSingleObject(osWrite.hEvent, INFINITE);
		switch (dwRes) {
		// OVERLAPPED structure's event has been signaled.
		case WAIT_OBJECT_0:
			if (!GetOverlappedResult(hComm, &osWrite, &dwWritten, FALSE))
				fRes = FALSE;
			else
				// Write operation completed successfully.
				fRes = TRUE;
			break;

		default:
			// An error has occurred in WaitForSingleObject.
			// This usually indicates a problem with the
			// OVERLAPPED structure's event handle.
			fRes = FALSE;
			break;
		}
	} else {
		// WriteFile completed immediately.
		fRes = TRUE;
	}

	CloseHandle(osWrite.hEvent);

	return fRes;
}


static int WriteData(unsigned char* data, int count)
{
	unsigned char buf[65];
	int i;

	memset(buf, 0, sizeof(buf));

	for (i = 0; i < count; ++i) {
		buf[i + 1] = data[i];
	}

	int result = WriteABuffer(&buf[0], count);

	printf("write %d, %d\r", count, result);

	return (result >= 0) ? 0 : -1;
}


static void ProgressUpdate(unsigned char arrayId, unsigned short rowNum)
{
	printf("\t\tCompleted array %d, row %d    \r", arrayId, rowNum);
}

TCHAR gszPort[] = L"COM2";

VOID CommSetBaud(VOID)
{
	if (hComm != NULL) {


		DCB dcb;

		ZeroMemory(&dcb, sizeof(dcb));
		dcb.DCBlength = sizeof(dcb);

		dcb.BaudRate = CBR_115200;
		dcb.fBinary = TRUE;
		dcb.fParity = TRUE;
		dcb.fOutxCtsFlow = FALSE;
		dcb.fOutxDsrFlow = FALSE;
		dcb.fDtrControl = DTR_CONTROL_DISABLE;
		dcb.fDsrSensitivity = FALSE;
		dcb.fOutX = FALSE;
		dcb.fErrorChar = FALSE;
		dcb.fNull = FALSE;
		dcb.fRtsControl = RTS_CONTROL_DISABLE;
		dcb.fAbortOnError = FALSE;
		dcb.ByteSize = 8;
		dcb.Parity = NOPARITY;
		dcb.StopBits = ONESTOPBIT;

		SetCommState(hComm, &dcb);
	}
	return;
}

int set_timeouts(void)
{
	COMMTIMEOUTS timeouts;

	if (hComm == NULL) return 1;

	timeouts.ReadIntervalTimeout = 1;
	timeouts.ReadTotalTimeoutMultiplier = 1;
	timeouts.ReadTotalTimeoutConstant = 1;
	timeouts.WriteTotalTimeoutMultiplier = 1;
	timeouts.WriteTotalTimeoutConstant = 1;
	if (!SetCommTimeouts(hComm, &timeouts)) {
		return 1;
	}
	return 0;

}

int main(int argc, char* argv[])
{

	hComm = CreateFile(gszPort,
	                   GENERIC_READ | GENERIC_WRITE,
	                   0,
	                   NULL,
	                   OPEN_EXISTING,
	                   FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED,
	                   NULL
	                  );

	if (hComm == INVALID_HANDLE_VALUE) {
		// error opening port; abort

		CloseHandle(hComm);

		exit(1);
	}

	CommSetBaud();

	set_timeouts();

	CyBtldr_CommunicationsData cyComms = {
		&OpenConnection,
		&CloseConnection,
		&ReadData,
		&WriteData,
		64
	};

	printf("Programming\n");

	int result = CyBtldr_Program(argv[1],
	                             &cyComms,
	                             &ProgressUpdate
	                            );

	printf("\nresult = %d\n", result);

	CloseHandle(hComm);

	return result;
}