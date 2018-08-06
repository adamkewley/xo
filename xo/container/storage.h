#pragma once

#include "xo/utility/types.h"
#include "xo/system/assert.h"
#include "xo/container/label_vector.h"

#include <vector>
#include <string>

namespace xo
{
	/// Simple storage class for storing frames with channels of data
	// TODO: use iterators for frames
	template< typename T, typename L = std::string >
	class storage
	{
	public:
		using value_type = T;
		using label_type = L;

		//struct const_frame {
		//	const_frame( const storage< T, L >& s, index_t f ) : sto_( s ), ofs_( f * s.channel_size() ) {}
		//	const T& operator[]( index_t i ) const { return sto_.data()[ ofs_ + i ]; }
		//	const T& operator[]( const L& l ) const { return sto_.data()[ ofs_ + sto_.find_channel( l ) ]; }
		//	const storage< T, L >& sto_;
		//	index_t ofs_;
		//};

		template< typename S >
		struct frame_ {
			frame_( S& s, index_t f ) : sto_( s ), frame_idx_( f ) {}
			auto& operator[]( index_t i ) const { return sto_( frame_idx_, i ); }
			auto& operator[]( const typename S::label_type& l ) const { return sto_( frame_idx_, l ); }
			S& sto_;
			index_t frame_idx_;
		};

		using frame = frame_< storage< T, L > >;
		using const_frame = frame_< const storage< T, L > >;

		struct const_frame_iterator {
			const_frame_iterator( const storage< T, L >& s, index_t f ) : sto_( s ), frame_idx_( f * s.channel_size() ) {}
			using iterator_category = std::forward_iterator_tag;
			using value_type = const_frame;
			using difference_type = int;
			using pointer = const_frame *;
			using reference = const_frame &;

			const_frame_iterator& operator++() { frame_idx_ += sto_.channel_size(); return *this; }
			const_frame_iterator operator++( int ) { auto keepit = *this; frame_idx_ += sto_.channel_size(); return keepit; }
			bool operator+( int i ) { frame_idx_ += i * sto_.channel_size(); }

			bool operator==( const const_frame_iterator& other ) { return frame_idx_ == other.frame_idx_; }
			bool operator!=( const const_frame_iterator& other ) { return frame_idx_ != other.frame_idx_; }
			const value_type operator*() const { return frame( sto_, frame_idx_ ); }
			const value_type operator->() const { return frame( sto_, frame_idx_ ); }

			const storage< T, L >& sto_;
			index_t frame_idx_;
		};

		storage( size_t frames = 0, size_t channels = 0, T value = T() ) : frame_size_( frames ), labels_( channels ), data_( channels * frames, value ) {}
		virtual ~storage() {}

		/// add a channel and resize buffer if needed
		index_t add_channel( L label, const T& value = T() ) { resize( frame_size(), channel_size() + 1, value ); return labels_.set( channel_size() - 1, label ); }

		/// add a channel with data, resize buffer if needed
		index_t add_channel( L label, const std::vector< T >& data ) {
			resize( std::max( frame_size(), data.size() ), channel_size() + 1 );
			labels_.set( channel_size() - 1, label );
			auto cidx = channel_size() - 1;
			for ( index_t fidx = 0; fidx < data.size(); ++fidx ) ( *this )( fidx, cidx ) = data[ fidx ];
			return cidx;
		}

		/// find index of a label
		index_t find_channel( const L& label ) const { return labels_.find_or_throw( label ); }

		/// find index of a label
		index_t try_find_channel( const L& label ) const { return labels_.find( label ); }

		/// find or add a channel
		index_t find_or_add_channel( const L& label, const T& value = T() ) {
			auto idx = labels_.find( label );
			return idx == no_index ? add_channel( label, value ) : idx;
		}

		/// set channel label
		void set_label( index_t channel, L label ) { labels_.set( channel, label ); }

		/// get channel label
		const L& get_label( index_t channel ) const { return labels_[ channel ]; }

		/// add frame to storage
		frame add_frame( T value = T( 0 ) ) { data_.resize( data_.size() + channel_size(), value ); ++frame_size_; return back(); }

		/// add frame to storage
		frame add_frame( const std::vector< T >& data ) {
			xo_assert( data.size() == channel_size() );
			data_.resize( data_.size() + channel_size() );
			++frame_size_;
			std::copy( data.begin(), data.end(), data_.end() - channel_size() );
			return back();
		}

		/// number of channels
		size_t channel_size() const { return labels_.size(); }

		/// number of frames in storage
		size_t frame_size() const { return frame_size_; }

		/// check if there is any data
		bool empty() const { return data_.empty(); }

		/// clear the storage
		void clear() { frame_size_ = 0; labels_.clear(); data_.clear(); }

		/// access value by channel index (no bounds checking)
		const T& operator()( index_t frame, index_t channel ) const { return data_[ frame * channel_size() + channel ]; }
		T& operator()( index_t frame, index_t channel ) { return data_[ frame * channel_size() + channel ]; }

		/// access value by channel label
		const T& operator()( index_t frame, const L& label ) const { return data_[ frame * channel_size() + find_channel( label ) ]; }
		T& operator()( index_t frame, const L& label ) { return data_[ frame * channel_size() + find_or_add_channel( label ) ]; }

		/// access frame
		const_frame operator[]( index_t f ) const { return const_frame( *this, f ); }
		frame operator[]( index_t f ) { return frame( *this, f ); }
		const_frame front() const { return const_frame( *this, 0 ); }
		frame front() { return frame( *this, 0 ); }
		const_frame back() const { return const_frame( *this, frame_size() - 1 ); }
		frame back() { return frame( *this, frame_size() - 1 ); }

		/// iterators
		const_frame_iterator begin() const { return const_frame_iterator( *this, 0 ); }
		const_frame_iterator end() const { return const_frame_iterator( *this, frame_size() ); }

		/// get the interpolated value of a specific frame / channel
		T get_interpolated_value( T frame_idx, index_t channel ) {
			auto frame_f = floor( frame_idx );
			auto frame_w = frame_idx - frame_f;
			index_t frame0 = static_cast<index_t>( frame_f );
			xo_assert( frame0 + 1 < frame_size() );
			index_t ofs = frame0 * this->channel_size() + channel;
			return ( T(1) - frame_w ) * data_[ ofs ] + frame_w * data_[ ofs + this->channel_size() ];
		}

		void resize( size_t nframes, size_t nchannels, T value = T() ) {
			xo_error_if( nframes < frame_size() || nchannels < channel_size(), "Cannot shrink storage" );
			if ( nchannels > channel_size() ) {
				if ( frame_size() > 1 ) {
					// reorganize existing data
					std::vector< T > new_data( nchannels * nframes, value );
					for ( index_t fi = 0; fi < frame_size(); ++fi )
						for ( index_t ci = 0; ci < channel_size(); ++ci )
							new_data[ nchannels * fi + ci ] = data_[ channel_size() * fi + ci ];
					data_ = std::move( new_data );
				}
				else data_.resize( nframes * nchannels, value ); // just resize

				labels_.resize( nchannels );
				frame_size_ = nframes;
			}
			else {
				// just resize data
				data_.resize( nframes * nchannels, value );
				frame_size_ = nframes;
			}
		}

		const label_vector< L >& labels() const { return labels_; }
		const std::vector< T >& data() const { return data_; }
		std::vector< T >& data() { return data_; }

	private:
		size_t frame_size_;
		label_vector< L > labels_;
		std::vector< T > data_;
	};

	template< typename T, typename L > std::ostream& operator<<( std::ostream& str, const storage< T, L >& buf ) {
		for ( index_t ci = 0; ci < buf.channel_size(); ++ci ) {
			str << buf.get_label( ci );
			if ( ci == buf.channel_size() - 1 ) str << std::endl; else str << '\t';
		}
		for ( index_t fi = 0; fi < buf.frame_size(); ++fi ) {
			for ( index_t ci = 0; ci < buf.channel_size(); ++ci ) {
				str << buf( fi, ci );
				if ( ci == buf.channel_size() - 1 ) str << std::endl; else str << '\t';
			}
		}
		return str;
	}
}
