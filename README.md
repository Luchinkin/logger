# logger
Just simple logger.

Usage example:
```cpp
	using namespace ::impl;

	logger::log( "Default log test\n" );
	{
		logger::scopped_padding_extender pad1( 10u );
		logger::log<wcon_green>( "pad1 and format test %f, %s\n", 0.5f, "str" );
		{
			logger::scopped_padding_extender pad2( 10u );
			logger::log<wcon_magenta>( "pad2\n" );
		}

		int i = 100;
		{
			logger::spin_waiter waiter_spin( true );

			while ( i != 0 )
			{
				waiter_spin.update();
				i--;
				Sleep( 10 );
			}
		}

		{
			logger::loading_waiter waiter_loading( 100 );
			while ( i <= 100 )
			{
				waiter_loading.update( i );
				i++;
				Sleep( 10 );
			}
		}
	}

	logger::error( "error\n" );
```


Preview:

![](preview.gif)
