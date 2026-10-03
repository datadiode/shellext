# shellext

This project implements a Windows Explorer shell extension to run arbitrary command lines which operate on selected files, utilizing a configurable number of worker processes to distribute work across CPU cores.

## Configuration example
```
HKCR
{
	NoRemove CLSID
	{
		ForceRemove {8545BEA4-83DF-4167-B67D-5FF71162F6D4} = s 'https://github.com/datadiode/shellext'
		{
			ForceRemove 'Programmable'
			InprocServer32 = s '%MODULE%'
			{
				val ThreadingModel = s 'Apartment'
			}
		}
	}
	NoRemove *
	{
		NoRemove ShellEx
		{
			NoRemove ContextMenuHandlers
			{
				ForceRemove {8545BEA4-83DF-4167-B67D-5FF71162F6D4} = s 'https://github.com/datadiode/shellext'
			}
		}
	}
}

[Format source code with clang-format;*.c;*.h;*.cpp;*.hpp;*.cxx;*.hxx]
command="clang-format.exe" -i "<filename>" ,1

[Format source code with pasfmt;*.pas;*.iss]
command="pasfmt.exe" "<filename>" ,1

[ping 127.0.0.1 to see the progress window]
command="ping.exe" "127.0.0.1" ,3
```

The configuration file is essentially an INI file which starts with an RGS script to control how the component registers with the operating system.
This creates an opportunity to register multiple instances of the same binary by assigning each one its own `CLSID`.
