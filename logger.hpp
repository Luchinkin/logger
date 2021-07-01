#pragma once
#include <iostream>
#include <cstdint>
#include <Windows.h>
#include <mutex>
#include <atomic>
#include <iomanip>
#include <array>

enum win_console_colors : std::uint8_t
{
	wcon_black				= 0,
	wcon_gray				= FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE, // DEFAULT
	wcon_white				= FOREGROUND_INTENSITY | FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE,
	wcon_red				= FOREGROUND_INTENSITY | FOREGROUND_RED,
	wcon_green				= FOREGROUND_INTENSITY | FOREGROUND_GREEN,
	wcon_blue				= FOREGROUND_INTENSITY | FOREGROUND_BLUE,
	wcon_cyan				= FOREGROUND_INTENSITY | FOREGROUND_GREEN | FOREGROUND_BLUE,
	wcon_magenta			= FOREGROUND_INTENSITY | FOREGROUND_RED | FOREGROUND_BLUE,
	wcon_yellow				= FOREGROUND_INTENSITY | FOREGROUND_RED | FOREGROUND_GREEN,
	wcon_dark_gray			= FOREGROUND_INTENSITY,
	wcon_dark_red			= FOREGROUND_RED,
	wcon_dark_green			= FOREGROUND_GREEN,
	wcon_dark_blue			= FOREGROUND_BLUE,
	wcon_dark_cyan			= FOREGROUND_GREEN | FOREGROUND_BLUE,
	wcon_dark_magenta		= FOREGROUND_RED | FOREGROUND_BLUE,
	wcon_dark_yellow		= FOREGROUND_RED | FOREGROUND_GREEN,
};

namespace impl::logger
{
	inline std::mutex logger_sync;
	inline std::atomic_uint8_t logger_padding = 0u;
	
	namespace utils
	{
		inline void setup_color( win_console_colors clr )
		{
			const auto out_handle = GetStdHandle( STD_OUTPUT_HANDLE );
			if ( out_handle == INVALID_HANDLE_VALUE )
				__debugbreak();
			else
				SetConsoleTextAttribute( GetStdHandle( STD_OUTPUT_HANDLE ), clr );
		}

		inline void reset_color()
		{
			setup_color( wcon_gray );
		}

		template <class T, std::size_t size>
		constexpr std::array<T, size> create_full_filled_array( T value )
		{
			std::array<T, size> result{};
			for ( std::size_t i = 0; i < size; ++i )
			{
				result[ i ] = value;
			}
			return result;
		}
	}

	/// <summary>
	/// - Prints text
	/// </summary>
	/// <typeparam name="...params"></typeparam>
	/// <param name="format">- text would be formatted using this</param>
	/// <param name="...parameters">- args for formating</param>
	/// <returns>printed characters count</returns>
	template<win_console_colors color = wcon_gray, typename... params>
	inline std::uint32_t log( const char* format, params&&... parameters )
	{
		std::lock_guard guard( logger_sync );

		std::string fmt_with_padding = "%*s";
		fmt_with_padding += format;

		utils::setup_color( color );
		const auto chars_count_ret = printf( fmt_with_padding.c_str(), logger_padding.load(), "", std::forward<params>( parameters )... );
		utils::reset_color();
		return chars_count_ret;
	}

	/// <summary>
	/// Colored(red) log with dbg break after print
	/// </summary>
	/// <typeparam name="...params"></typeparam>
	/// <param name="format">- text would be formatted using this</param>
	/// <param name="...parameters">- args for formatting</param>
	template<typename... params>
	inline void error( const char* format, params&&... parameters )
	{
		log<wcon_red>( format, std::forward<params>( parameters )... );

		__debugbreak();
	}

	/// <summary>
	/// Scopped padding extender(prev_padding += current_padding for this scope)
	/// </summary>
	struct scopped_padding_extender
	{
		std::uint8_t restore_pad = 0u;

		scopped_padding_extender( std::uint8_t current_pad )
		{
			restore_pad = logger_padding;
			logger_padding.fetch_add( current_pad, std::memory_order_relaxed );
		}

		~scopped_padding_extender()
		{
			logger_padding = restore_pad;
		}
	};

	/// <summary>
	/// Scopped spin waiter
	/// </summary>
	struct spin_waiter
	{
		static constexpr std::array<std::uint8_t, 4> animation = { '\\', '|', '/', 0xC4 };
		static constexpr std::uint8_t update_rate_ms = 100u;
		
		std::uint8_t sequence = 0u;
		std::chrono::steady_clock::time_point last_update_time;

		bool should_clear_on_release = false;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="t_should_clear_on_release">Set it to true, if you want to clear last cmd line from this widget</param>
		/// <returns></returns>
		spin_waiter( bool t_should_clear_on_release = false ) : should_clear_on_release( t_should_clear_on_release )
		{
			logger_sync.lock();
			last_update_time = std::chrono::high_resolution_clock::now();
		}

		~spin_waiter()
		{
			printf( should_clear_on_release ? "\r" : "\n" );

			logger_sync.unlock();
		}

		/// <summary>
		/// Updater/Renderer
		/// </summary>
		void update()
		{

			const auto delta = std::chrono::duration_cast<std::chrono::milliseconds>( std::chrono::high_resolution_clock::now() - last_update_time );
			if ( delta.count() < update_rate_ms )
				return;

			sequence = ( sequence + 1 ) % sizeof( animation );

			printf( "%*s%c\r", logger_padding.load(), "", animation[ sequence ] );

			last_update_time = std::chrono::high_resolution_clock::now();
		}
	};

	/// <summary>
	/// Scopped loading waiter
	/// </summary>
	/// <typeparam name="T"></typeparam>
	template<class T>
	struct loading_waiter
	{
		static constexpr auto full_line = utils::create_full_filled_array<std::uint8_t, 10>( 0xFE );

		std::uint32_t last_drawn_count = 0u;

		T max_value;

		bool should_clear_on_release = false;

		/// <summary>
		/// Contructor
		/// </summary>
		/// <param name="t_max_value">- max value</param>
		/// <param name="t_should_clear_on_release">Set it to true, if you want to clear last cmd line from this widget</param>
		/// <returns></returns>
		loading_waiter( T t_max_value, bool t_should_clear_on_release = false ) : max_value( t_max_value ),
			should_clear_on_release( t_should_clear_on_release )
		{
			logger_sync.lock();
		}

		~loading_waiter()
		{
			if ( should_clear_on_release )
				printf( "%*s\r", last_drawn_count, "" );
			else
				printf( "\n" );

			logger_sync.unlock();
		}

		/// <summary>
		/// Updater/Renderer
		/// </summary>
		/// <param name="current_value"></param>
		void update( T current_value )
		{
			static constexpr auto length = full_line.size();
			const auto percentage = std::min<T>( current_value, max_value ) / static_cast<float>( max_value );
			const int percent = static_cast<int>( percentage * 100 );
			const int count_to_draw = static_cast<int>( percentage * length );
			const int count_to_skip = length - count_to_draw;

			last_drawn_count = printf( "\r%*s%d%%[%.*s%*s]", logger_padding.load(), "",percent, count_to_draw, full_line.data(), count_to_skip, "" );
		}
	};
}
