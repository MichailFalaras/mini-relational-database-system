export const DBMS_ERRORS = {
	"INVALID_REQUEST": "protocol", 
	"UNSUPPORTED_VERSION": "protocol", 
	"UNSUPPORTED_OPERATION": "protocol", 
	"MESSAGE_TOO_LARGE": "protocol",
	"DATABASE_NOT_OPEN": "semantic", 
	"DATABASE_ALREADY_OPEN": "semantic", 
	"DATABASE_ALREADY_EXISTS": "semantic", 
	"DATABASE_NOT_FOUND": "semantic",
	"DATABASE_INVALID_FORMAT": "storage", 
	"DATABASE_IN_USE": "semantic", 
	"SQL_SYNTAX_ERROR": "syntax", 
	"TABLE_NOT_FOUND": "semantic",
	"COLUMN_NOT_FOUND": "semantic", 
	"CONSTRAINT_VIOLATION": "constraint", 
	"STORAGE_ERROR": "storage", 
	"RESULT_TOO_LARGE": "internal", 
	"INTERNAL_ERROR": "internal"
};