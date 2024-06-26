/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.
Copyright (C) 2017 Noah Metzger (chomenor@gmail.com)

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake III Arena source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/

#if defined( CMOD_COPYDEBUG_CMD_SUPPORTED ) || defined( CMOD_VM_PERMISSIONS ) || defined( CMOD_IMPORT_SETTINGS ) \
		|| defined ( CMOD_MARIO_MOD_FIX )
#include "../filesystem/fslocal.h"
#else
#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"
#endif

#if defined( CMOD_URI_REGISTER_COMMAND ) && defined( PROTOCOL_HANDLER) && defined( _WIN32 ) && !defined( UNICODE )
#define URI_REGISTER_SUPPORTED
#endif

#if defined _WIN32 && ( defined( CMOD_COPYDEBUG_CMD_SUPPORTED ) || defined( CMOD_IMPORT_SETTINGS ) \
		|| defined( URI_REGISTER_SUPPORTED ) )
#include <windows.h>
#endif

#ifdef CMOD_COMMON_STRING_FUNCTIONS
void cmod_stream_append_string(cmod_stream_t *stream, const char *string) {
	// If stream runs out of space, output is truncated.
	// Non-zero size stream will always be null terminated.
	if(stream->position >= stream->size) {
		if(stream->size) stream->data[stream->size-1] = 0;
		stream->overflowed = qtrue;
		return; }
	if(string) while(*string) {
		if(stream->position >= stream->size-1) {
			stream->overflowed = qtrue;
			break; }
		stream->data[stream->position++] = *(string++); }
	stream->data[stream->position] = 0; }

void cmod_stream_append_string_separated(cmod_stream_t *stream, const char *string, const char *separator) {
	// Appends string, adding separator prefix if both stream and input are non-empty
	if(stream->position && string && *string) cmod_stream_append_string(stream, separator);
	cmod_stream_append_string(stream, string); }

void cmod_stream_append_data(cmod_stream_t *stream, const char *data, unsigned int length) {
	// Appends bytes to stream. Does not add null terminator.
	if(stream->position > stream->size) {
		stream->overflowed = qtrue;
		return; }
	if(length > stream->size - stream->position) {
		length = stream->size - stream->position;
		stream->overflowed = qtrue; }
	Com_Memcpy(stream->data + stream->position, data, length);
	stream->position += length; }

#define IS_WHITESPACE(chr) ((chr) == ' ' || (chr) == '\t' || (chr) == '\n' || (chr) == '\r')

unsigned int cmod_read_token(const char **current, char *buffer, unsigned int buffer_size, char delimiter) {
	// Returns number of characters read to output buffer (not including null terminator)
	// Null delimiter uses any whitespace as delimiter
	// Any leading and trailing whitespace characters will be skipped
	unsigned int char_count = 0;

	// Skip leading whitespace
	while(**current && IS_WHITESPACE(**current)) ++(*current);

	// Read item to buffer
	while(**current && **current != delimiter) {
		if(!delimiter && IS_WHITESPACE(**current)) break;
		if(buffer_size && char_count < buffer_size - 1) {
			buffer[char_count++] = **current; }
		++(*current); }

	// Skip input delimiter and trailing whitespace
	if(**current) ++(*current);
	while(**current && IS_WHITESPACE(**current)) ++(*current);

	// Skip output trailing whitespace
	while(char_count && IS_WHITESPACE(buffer[char_count-1])) --char_count;

	// Add null terminator
	if(buffer_size) buffer[char_count] = 0;
	return char_count; }

unsigned int cmod_read_token_ws(const char **current, char *buffer, unsigned int buffer_size) {
	// Reads whitespace-separated token from current to buffer, and advances current pointer
	// Returns number of characters read to buffer (not including null terminator, 0 if no data remaining)
	return cmod_read_token(current, buffer, buffer_size, 0); }
#endif

#ifdef CMOD_VM_STRNCPY_FIX
// Simple strncpy function to avoid overlap check issues with some library implementations
void vm_strncpy(char *dst, char *src, int length) {
	int i;
	for(i=0; i<length; ++i) {
		dst[i] = src[i];
		if(!src[i]) break; }
	for(; i<length; ++i) {
		dst[i] = 0; } }
#endif

#ifdef CMOD_ANTI_BURNIN
float cmod_anti_burnin_shift(float val) {
	if(cmod_anti_burnin->value <= 0.0f) return val;
	if(cmod_anti_burnin->value >= 1.0f) return 0.5f;
	float result = val * (1.0f - cmod_anti_burnin->value) + 0.5f * cmod_anti_burnin->value;
	if(result < 0.0f) result = 0.0f;
	if(result > 1.0f) result = 1.0f;
	return result; }
#endif

#ifdef CMOD_COPYDEBUG_CMD
#ifdef CMOD_COPYDEBUG_CMD_SUPPORTED
static void cmod_debug_get_config(cmod_stream_t *stream) {
	// Based on FS_ExecuteConfigFile
	char *data = 0;
	char path[FS_MAX_PATH];
	if(FS_GeneratePathSourcedir(0, "cmod.cfg", 0, FS_ALLOW_SPECIAL_CFG, 0, path, sizeof(path))) {
		data = FS_ReadData(0, path, 0, "cmod_debug_get_config"); }

	cmod_stream_append_string_separated(stream, data ? data : "[file not found]", "\n"); }

static void cmod_debug_get_autoexec(cmod_stream_t *stream) {
	// Based on FS_ExecuteConfigFile
	const fsc_file_t *file;
	const char *data = 0;
	unsigned int size;
	int lookup_flags = LOOKUPFLAG_PURE_ALLOW_DIRECT_SOURCE | LOOKUPFLAG_IGNORE_CURRENT_MAP;
	if(fs.cvar.fs_download_mode->integer >= 2) {
		// Don't allow config files from restricted download folder pk3s, because they could disable the download folder
		// restrictions to unrestrict themselves
		lookup_flags |= LOOKUPFLAG_NO_DOWNLOAD_FOLDER; }
	// For q3config.cfg and autoexec.cfg - only load files on disk and from appropriate fs_mod_settings locations
	lookup_flags |= (LOOKUPFLAG_SETTINGS_FILE | LOOKUPFLAG_DIRECT_SOURCE_ONLY);

	file = FS_GeneralLookup("autoexec.cfg", lookup_flags, qfalse);
	if(file) {
		data = FS_ReadData(file, 0, &size, "cmod_debug_get_autoexec"); }

	cmod_stream_append_string_separated(stream, data ? data : "[file not found]", "\n"); }

static void cmod_debug_get_filelist(cmod_stream_t *stream) {
	fsc_file_iterator_t it = FSC_FileIteratorOpenAll(&fs.index);
	char buffer[FS_FILE_BUFFER_SIZE];

	while(FSC_FileIteratorAdvance(&it)) {
		if(it.file->sourcetype != FSC_SOURCETYPE_DIRECT) continue;

		FS_FileToBuffer(it.file, buffer, sizeof(buffer), qtrue, qtrue, qtrue, qfalse);
		cmod_stream_append_string_separated(stream, buffer, "\n");

		if(((fsc_file_direct_t *)it.file)->pk3_hash) {
			cmod_stream_append_string(stream, va(" (hash:%i)", (int)((fsc_file_direct_t *)it.file)->pk3_hash)); } } }

static void copydebug_write_clipboard(cmod_stream_t *stream) {
	// Based on sys_win32.c->Sys_ErrorDialog
	HGLOBAL memoryHandle;
	char *clipMemory;

	memoryHandle = GlobalAlloc( GMEM_MOVEABLE|GMEM_DDESHARE, stream->position+1);
	clipMemory = (char *)GlobalLock( memoryHandle );

	if( clipMemory )
	{
		Com_Memcpy(clipMemory, stream->data, stream->position);
		clipMemory[stream->position+1] = 0;

		if( OpenClipboard( NULL ) && EmptyClipboard( ) )
			SetClipboardData( CF_TEXT, memoryHandle );

		GlobalUnlock( clipMemory );
		CloseClipboard( );
	}
}
#endif

void cmod_copydebug_cmd(void) {
#ifdef CMOD_COPYDEBUG_CMD_SUPPORTED
	char buffer[65536];
	cmod_stream_t stream = {buffer, 0, sizeof(buffer), qfalse};

	cmod_stream_append_string_separated(&stream, "console history\n>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>", "\n\n");
	cmod_debug_get_console(&stream);
	cmod_stream_append_string_separated(&stream, "<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<", "\n");

	cmod_stream_append_string_separated(&stream, "cmod.cfg\n>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>", "\n\n");
	cmod_debug_get_config(&stream);
	cmod_stream_append_string_separated(&stream, "<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<", "\n");

	cmod_stream_append_string_separated(&stream, "autoexec.cfg\n>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>", "\n\n");
	cmod_debug_get_autoexec(&stream);
	cmod_stream_append_string_separated(&stream, "<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<", "\n");

	if ( !Q_stricmp( Cmd_Argv(1), "files" ) ) {
		cmod_stream_append_string_separated(&stream, "file list\n>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>", "\n\n");
		cmod_debug_get_filelist(&stream);
		cmod_stream_append_string_separated(&stream, "<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<", "\n");
	}

	cmod_stream_append_string_separated(&stream, "End of debug output.", "\n\n");
	copydebug_write_clipboard(&stream);
	Com_Printf("Debug info copied to clipboard.\n");
#else
	Com_Printf("Command not supported on this operating system or build configuration.\n");
#endif
}
#endif

#ifdef CMOD_CLIENT_ALT_SWAP_SUPPORT
#define EF_BUTTON_ATTACK 1
#define EF_BUTTON_ALT_ATTACK 32

static qboolean clientAltSwapActive;

void ClientAltSwap_CGameInit( void ) {
	// Don't leave settings from a previous mod
	clientAltSwapActive = qfalse;
}

void ClientAltSwap_ModifyCommand( usercmd_t *cmd ) {
	if ( clientAltSwapActive ) {
		if ( cmd->buttons & EF_BUTTON_ALT_ATTACK ) {
			cmd->buttons &= ~EF_BUTTON_ALT_ATTACK;
			cmd->buttons |= EF_BUTTON_ATTACK;
		} else if ( cmd->buttons & EF_BUTTON_ATTACK ) {
			cmd->buttons |= ( EF_BUTTON_ATTACK | EF_BUTTON_ALT_ATTACK );
		}
	}
}

void ClientAltSwap_SetState( qboolean swap ) {
	clientAltSwapActive = swap;
}
#endif

#ifdef CMOD_VM_PERMISSIONS
qboolean FS_CheckTrustedVMFile( const fsc_file_t *file );
static qboolean vmTrusted[VM_MAX];
static const fsc_file_t *initialUI = NULL;

/*
=================
VMPermissions_CheckTrustedVMFile

Returns qtrue if VM file is trusted.
=================
*/
qboolean VMPermissions_CheckTrustedVMFile( const fsc_file_t *file, const char *debug_name ) {
	// Download folder pk3s are checked by hash
	if ( file && FSC_FromDownloadPk3( file, &fs.index ) ) {
		if ( FS_CheckTrustedVMFile( file ) ) {
			if ( debug_name ) {
				Com_Printf( "Downloaded module '%s' trusted due to known mod hash.\n", debug_name );
			}
			return qtrue;
		}

		// Always trust the first loaded UI, to avoid situations with irregular configs where the
		// default UI is restricted. This shouldn't affect security much because if the default UI
		// is compromised there are already significant problems.
		if ( initialUI == file ) {
			if ( debug_name ) {
				Com_Printf( "Downloaded module '%s' trusted due to matching initial selected UI.\n", debug_name );
			}
			return qtrue;
		}

		if ( debug_name ) {
			Com_Printf( "Downloaded module '%s' restricted. Some settings may not be saved.\n", debug_name );
		}
		return qfalse;
	}

	// Other types are automatically trusted
	return qtrue;
}

/*
=================
VMPermissions_OnVmCreate

Called when a VM is about to be instantiated. sourceFile may be null in error cases.
=================
*/
void VMPermissions_OnVmCreate( const char *module, const fsc_file_t *sourceFile, qboolean is_dll ) {
	vmType_t vmType = VM_NONE;
	if ( !Q_stricmp( module, "qagame" ) ) {
		vmType = VM_GAME;
	} else if ( !Q_stricmp( module, "cgame" ) ) {
		vmType = VM_CGAME;
	} else if ( !Q_stricmp( module, "ui" ) ) {
		vmType = VM_UI;
	} else {
		return;
	}

	// Save first loaded UI
	if ( vmType == VM_UI && !initialUI ) {
		initialUI = sourceFile;
	}

	// Check if VM is trusted
	vmTrusted[vmType] = VMPermissions_CheckTrustedVMFile( sourceFile, module );
}
#endif

#ifdef CMOD_CORE_VM_PERMISSIONS
/*
=================
VMPermissions_CheckTrusted

Returns whether currently loaded VM is trusted.
=================
*/
qboolean VMPermissions_CheckTrusted( vmType_t vmType ) {
#ifdef CMOD_VM_PERMISSIONS
	if ( vmType <= VM_NONE || vmType >= VM_MAX ) {
		Com_Printf( "WARNING: VMPermissions_CheckTrusted with invalid vmType\n" );
		return qfalse;
	}

	return vmTrusted[vmType];
#else
	return qtrue;
#endif
}
#endif

#ifdef CMOD_CLIENT_MODCFG_HANDLING
modCfgValues_t ModcfgHandling_CurrentValues;

/*
=================
ModcfgHandling_ParseModConfig

Called when gamestate is received from the server.
=================
*/
void ModcfgHandling_ParseModConfig( int *stringOffsets, char *data ) {
	int i;
	char key[BIG_INFO_STRING];
	char value[BIG_INFO_STRING];

	memset( &ModcfgHandling_CurrentValues, 0, sizeof( ModcfgHandling_CurrentValues ) );

	// look for any configstring matching "!modcfg " prefix
	for ( i = 0; i < MAX_CONFIGSTRINGS; ++i ) {
		const char *str = data + stringOffsets[i];

		if ( str[0] == '!' && !Q_stricmpn( str, "!modcfg ", 8 ) && strlen( str ) < BIG_INFO_STRING ) {
			const char *cur = &str[8];

			// load values
			while ( 1 ) {
				Info_NextPair( &cur, key, value );
				if ( !key[0] )
					break;
#ifdef CMOD_QVM_SELECTION
				if ( !Q_stricmp( key, "nativeUI" ) )
					ModcfgHandling_CurrentValues.nativeUI = atoi( value );
				if ( !Q_stricmp( key, "nativeCgame" ) )
					ModcfgHandling_CurrentValues.nativeCgame = atoi( value );
#endif
			}
		}
	}
}
#endif

#ifdef CMOD_IMPORT_SETTINGS
#ifdef _WIN32
#include <shlobj.h>
#endif
#include <stdio.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>

/*
=================
Stef_ImportSettings_GetModifiedTime

Retrieve file modification time. Returns 0 on error or file not found.
=================
*/
static long long Stef_ImportSettings_GetModifiedTime( const char *path ) {
	struct stat st = { 0 };
	if ( stat( path, &st ) == -1 ) {
		return 0;
	}
	return st.st_mtime;
}

/*
=================
Stef_ImportSettings_GetCurrentTime

Retrieve system time in same format as modification time.
=================
*/
static long long Stef_ImportSettings_GetCurrentTime( void ) {
	time_t ltime = 0;
	time( &ltime );
	return ltime;
}

typedef struct {
	long long modifiedTime;
	char path[FS_MAX_PATH];
	qboolean cmodFormat;
} SettingsImportSource_t;

/*
=================
Stef_ImportSettings_AddSource

Checks a potential config file location. If file exists and has a more recent modified timestamp than
the current best source file, replace the source file with this one.
=================
*/
static void Stef_ImportSettings_AddSource( const char *path, qboolean cmodFormat, SettingsImportSource_t *target ) {
	long long modifiedTime = Stef_ImportSettings_GetModifiedTime( path );
	if ( modifiedTime ) {
		Com_Printf( "Potential import config: %s (modified: %u)\n", path, (unsigned int)modifiedTime );
	}

	if ( modifiedTime && modifiedTime > target->modifiedTime ) {
		target->modifiedTime = modifiedTime;
		Q_strncpyz( target->path, path, sizeof( target->path ) );
		target->cmodFormat = cmodFormat;
	}
}

/*
=================
Stef_ImportSettings_GetSource

Search potential config file locations and find the one with the most recent timestamp.
=================
*/
static void Stef_ImportSettings_GetSource( SettingsImportSource_t *target ) {
	int i;
	char path[FS_MAX_PATH];
	memset( target, 0, sizeof( *target ) );

	// Check filesystem search locations
	for ( i = 1; i < FS_MAX_SOURCEDIRS; ++i ) {
		if ( fs.sourcedirs[i].active &&
				FS_GeneratePathSourcedir( i, "cmod.cfg", NULL, FS_NO_SANITIZE, 0, path, sizeof( path ) ) ) {
			Stef_ImportSettings_AddSource( path, qtrue, target );
		}
	}

	for ( i = 0; i < FS_MAX_SOURCEDIRS; ++i ) {
		if ( fs.sourcedirs[i].active &&
				FS_GeneratePathSourcedir( i, "baseEF/hmconfig.cfg", NULL, FS_NO_SANITIZE, 0, path, sizeof( path ) ) ) {
			Stef_ImportSettings_AddSource( path, qfalse, target );
		}
	}

#ifdef _WIN32
	// Check user directory (based on Sys_DefaultHomePath)
	{
		HMODULE shfolder = LoadLibrary( "shfolder.dll" );

		if ( shfolder ) {
			TCHAR szPath[MAX_PATH];
			FARPROC qSHGetFolderPath = GetProcAddress( shfolder, "SHGetFolderPathA" );

			if ( qSHGetFolderPath ) {
				if ( SUCCEEDED( qSHGetFolderPath( NULL, CSIDL_APPDATA, NULL, 0, szPath ) ) ) {
					if ( FS_GeneratePath( szPath, "Lilium Voyager/baseEF/hmconfig.cfg",
							NULL, FS_NO_SANITIZE, FS_NO_SANITIZE, 0, path, sizeof( path ) ) ) {
						Stef_ImportSettings_AddSource( path, qfalse, target );
					}

					if ( FS_GeneratePath( szPath, "Tulip Voyager/baseEF/hmconfig.cfg",
							NULL, FS_NO_SANITIZE, FS_NO_SANITIZE, 0, path, sizeof( path ) ) ) {
						Stef_ImportSettings_AddSource( path, qfalse, target );
					}
				}

				if ( SUCCEEDED( qSHGetFolderPath( NULL, CSIDL_LOCAL_APPDATA, NULL, 0, szPath ) ) ) {
					if ( FS_GeneratePath( szPath,
							"VirtualStore/Program Files (x86)/Raven/Star Trek Voyager Elite Force/BaseEF/hmconfig.cfg",
							NULL, FS_NO_SANITIZE, FS_NO_SANITIZE, 0, path, sizeof( path ) ) ) {
						Stef_ImportSettings_AddSource( path, qfalse, target );
					}
				}
			}

			FreeLibrary( shfolder );
		}
	}
#endif
}

/*
=================
Stef_ImportSettings_CheckImport

Look for potential config files to import and display prompt if one is found.
Called when the standard cmod.cfg doesn't exist.
=================
*/
void Stef_ImportSettings_CheckImport( void ) {
	SettingsImportSource_t source;
	Stef_ImportSettings_GetSource( &source );

	if ( source.modifiedTime ) {
		long long currentTime = Stef_ImportSettings_GetCurrentTime();
		if ( currentTime - source.modifiedTime > 30 * 24 * 60 * 60 ) {
			// Don't bother prompting about files older than about 30 days
			Com_Printf( "Import config too old: file modified time %u, system time %u\n",
					(unsigned int)source.modifiedTime, (unsigned int)currentTime );

		} else {
			// Found a valid config to import
			if ( Sys_Dialog( DT_YES_NO,
					va( "An existing config was found at\n\n%s\n\nImport settings from this file?",
					source.path ), "Settings Import" ) == DR_YES ) {
				char *data = FS_ReadData( NULL, source.path, NULL, __FUNCTION__ );
				if ( data ) {
					// Exec cMod configs directly, but apply filtering to ones from other clients
					cmd_mode_t mode = source.cmodFormat ? CMD_NORMAL : CMD_SETTINGS_IMPORT;
					Com_Printf( "Importing settings: %s\n", source.path );
					Cbuf_ExecuteTextByMode( EXEC_APPEND, data, mode );
					Cbuf_ExecuteTextByMode( EXEC_APPEND, "\n", mode );
					FS_FreeData( data );
				}
			}
		}
	}
}
#endif

#ifdef CMOD_MARIO_MOD_FIX
qboolean Stef_MarioModFix_ModActive = qfalse;

void Stef_MarioModFix_OnVMCreate( const char *module, const fsc_file_t *sourceFile ) {
	if ( !Q_stricmp( module, "qagame" ) ) {
		Stef_MarioModFix_ModActive = qfalse;

		if ( sourceFile && sourceFile->filesize == 672876 ) {
			unsigned int size = 0;
			char *data = FS_ReadData( sourceFile, NULL, &size, __func__ );
			if ( data ) {
				unsigned int hash = Com_BlockChecksum( data, size );
				FS_FreeData( data );
				if ( hash == 2630475216u ) {
					Com_Printf( "Enabling engine workaround for Mario Mod connection issues.\n" );
					Stef_MarioModFix_ModActive = qtrue;
				}
			}
		}
	}
}
#endif

#ifdef CMOD_URI_REGISTER_COMMAND
#ifdef URI_REGISTER_SUPPORTED
#include <winreg.h>

/*
=================
Stef_UriCmd_Remove

Remove URI handler.
=================
*/
void Stef_UriCmd_Remove(void) {
	// Delete keys individually, rather than using RegDeleteTree. This provides a bit of extra safety,
	// as the operation will fail if there are any unexpected subkeys created by a different application.
	LSTATUS result;
	RegDeleteKeyA( HKEY_CURRENT_USER, "Software\\Classes\\" PROTOCOL_HANDLER "\\shell\\open\\command" );
	RegDeleteKeyA( HKEY_CURRENT_USER, "Software\\Classes\\" PROTOCOL_HANDLER "\\shell\\open" );
	RegDeleteKeyA( HKEY_CURRENT_USER, "Software\\Classes\\" PROTOCOL_HANDLER "\\shell" );
	RegDeleteKeyA( HKEY_CURRENT_USER, "Software\\Classes\\" PROTOCOL_HANDLER "\\DefaultIcon" );
	result = RegDeleteKeyA( HKEY_CURRENT_USER, "Software\\Classes\\" PROTOCOL_HANDLER );

	if ( result == ERROR_SUCCESS || result == ERROR_FILE_NOT_FOUND ) {
		Com_Printf( "^3Successfully removed handler at HKEY_CURRENT_USER\\Software\\Classes\\" PROTOCOL_HANDLER "\n" );
		Com_Printf( "If you wish to restore it, use the command \"uri register\".\n" );
	} else {
		Com_Printf( "^3Error: Protocol handler may not be fully removed.\n" );
		Com_Printf( "To ensure complete removal, open the Windows Registry Editor and manually delete the key "
				"at HKEY_CURRENT_USER\\Software\\Classes\\" PROTOCOL_HANDLER "\n" );
	}
}

/*
=================
Stef_UriCmd_WriteRegistryValue

Writes a value to the registry, creating key if it doesn't already exist.
valueName may be NULL to set unnamed value.
=================
*/
static void Stef_UriCmd_WriteRegistryValue( HKEY rootKey, const char *subKey, const char *valueName, const char *value ) {
	HKEY hKey = NULL;
	DWORD dontcare = 0;
	LSTATUS result;

	result = RegCreateKeyExA( rootKey, subKey, 0, NULL, 0, KEY_SET_VALUE, NULL, &hKey, &dontcare );
	if ( result == ERROR_SUCCESS ) {
		LSTATUS result2;

		result2 = RegSetValueExA( hKey, valueName, 0, REG_SZ, (const BYTE *)value, strlen( value ) + 1 );
		if ( result2 != ERROR_SUCCESS ) {
			Com_Printf( "RegSetValueExA error: subKey(%s) valueName(%s) value(%s) error(%i)\n",
					subKey, valueName ? valueName : "<null>", value, (int)result2 );
		}

		result2 = RegCloseKey( hKey );
		if ( result2 != ERROR_SUCCESS ) {
			Com_Printf( "RegCloseKey error: subKey(%s) valueName(%s) value(%s) error(%i)\n",
					subKey, valueName ? valueName : "<null>", value, (int)result2 );
		}
	} else {
		Com_Printf( "RegCreateKeyExA error: subKey(%s) valueName(%s) value(%s) error(%i)\n",
				subKey, valueName ? valueName : "<null>", value, (int)result );
	}
}

/*
=================
Stef_UriCmd_Register

Register URI handler.
=================
*/
static void Stef_UriCmd_Register(void) {
	char appPath[MAX_PATH];
	appPath[0] = '\0';

    if ( !GetModuleFileNameA( NULL, appPath, MAX_PATH ) ) {
		Com_Printf( "Failed to retrieve app path (%i)\n", (int)GetLastError() );
		return;
	}

	if ( !FS_FileInPathExists( appPath ) ) {
		Com_Printf( "Failed to verify app path\n" );
		return;
	}

	Stef_UriCmd_WriteRegistryValue( HKEY_CURRENT_USER, "Software\\Classes\\" PROTOCOL_HANDLER, "CustomUrlApplication", appPath );
	Stef_UriCmd_WriteRegistryValue( HKEY_CURRENT_USER, "Software\\Classes\\" PROTOCOL_HANDLER, "CustomUrlArguments", "\"%1\"" );
	Stef_UriCmd_WriteRegistryValue( HKEY_CURRENT_USER, "Software\\Classes\\" PROTOCOL_HANDLER, "URL Protocol", "" );
	Stef_UriCmd_WriteRegistryValue( HKEY_CURRENT_USER, "Software\\Classes\\" PROTOCOL_HANDLER "\\DefaultIcon", NULL,
			va( "%s,0", appPath ) );
	Stef_UriCmd_WriteRegistryValue( HKEY_CURRENT_USER, "Software\\Classes\\" PROTOCOL_HANDLER "\\shell\\open\\command", NULL,
			va( "\"%s\" --uri \"%%1\"", appPath ) );

	Com_Printf( "^3Registered protocol " PROTOCOL_HANDLER ":// at HKEY_CURRENT_USER\\Software\\Classes\\" PROTOCOL_HANDLER "\n" );
	Com_Printf( "^3You should now be able to join servers using links in your web browser.\n" );
	Com_Printf( "If you wish to reverse this, use the command \"uri remove\".\n" );
}
#endif

/*
=================
Stef_UriCmd

Handle "uri" console command.
=================
*/
void Stef_UriCmd( void ) {
#ifdef URI_REGISTER_SUPPORTED
	const char *arg = Cmd_Argv( 1 );
	if ( !Q_stricmp( arg, "register" ) ) {
		Stef_UriCmd_Register();
	} else if ( !Q_stricmp( arg, "remove" ) ) {
		Stef_UriCmd_Remove();
	} else {
		Com_Printf( "Usage: uri [register|remove]\n^3uri register^7: Registers URI handler to support joining "
				"servers through links in your web browser.\n^3uri remove^7: Removes URI handler.\n" );
	}
#else
#ifdef _WIN32
	Com_Printf( "Not built with URI handler support.\n" );
#else
	Com_Printf( "URI register command is currently only supported on Windows.\n" );
#endif
#endif
}
#endif
