/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright (C) 2017 CERN
 * Copyright (C) 2019-2021 KiCad Developers, see AUTHORS.txt for contributors.
 *
 * @author Maciej Suminski <maciej.suminski@cern.ch>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 3
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, you may find one here:
 * https://www.gnu.org/licenses/gpl-3.0.html
 * or you may search the http://www.gnu.org website for the version 3 license,
 * or you may write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 */

#ifndef SYMBOL_LIBRARY_MANAGER_H
#define SYMBOL_LIBRARY_MANAGER_H

#include <map>
#include <list>
#include <deque>
#include <set>
#include <memory>
#include <wx/arrstr.h>
#include <symbol_tree_synchronizing_adapter.h>
#include <sch_io_mgr.h>
#include <sch_screen.h>

class LIB_PART;
class PART_LIB;
class SCH_PLUGIN;
class SYMBOL_EDIT_FRAME;
class SYMBOL_LIB_TABLE;
class SYMBOL_LIB_TABLE_ROW;


class LIB_LOGGER : public wxLogGui
{
public:
    LIB_LOGGER() :
            m_previousLogger( nullptr ),
            m_activated( false )
    { }

    ~LIB_LOGGER() override
    {
        Deactivate();
    }

    void Activate()
    {
        if( !m_activated )
        {
            m_previousLogger = wxLog::GetActiveTarget();
            wxLog::SetActiveTarget( this );
            m_activated = true;
        }
    }

    void Deactivate()
    {
        if( m_activated )
        {
            Flush();
            m_activated = false;
            wxLog::SetActiveTarget( m_previousLogger );
        }
    }

    void Flush() override
    {
        if( m_bHasMessages )
        {
            wxLogMessage( _( "Not all symbol libraries could be loaded.  Use the Manage Symbol\n"
                             "Libraries dialog to adjust paths and add or remove libraries." ) );
            wxLogGui::Flush();
        }
    }

private:
    wxLog* m_previousLogger;
    bool   m_activated;
};


/**
 * Class to handle modifications to the symbol libraries.
 */
class SYMBOL_LIBRARY_MANAGER
{
public:
    SYMBOL_LIBRARY_MANAGER( SYMBOL_EDIT_FRAME& aFrame );

    /**
     * Updates the #SYMBOL_LIBRARY_MANAGER data to synchronize with Symbol Library Table.
     */
    void Sync( bool aForce = false,
               std::function<void( int, int, const wxString& )> aProgressCallback
                    = []( int, int, const wxString& )
                      {
                      } );

    int GetHash() const;

    bool HasModifications() const;

    /**
     * Return a library hash value to determine if it has changed.
     *
     * For buffered libraries, it returns a number corresponding to the number of modifications.
     * For original libraries, hash is computed basing on the library URI. Returns -1 when the
     * requested library does not exist.
     */
    int GetLibraryHash( const wxString& aLibrary ) const;

    /**
     * Return the array of library names.
     */
    wxArrayString GetLibraryNames() const;

    /**
     * Find a single library within the (aggregate) library table.
     */
    SYMBOL_LIB_TABLE_ROW* GetLibrary( const wxString& aLibrary ) const;

    std::list<LIB_PART*> GetAliases( const wxString& aLibrary ) const;

    /**
     * Create an empty library and adds it to the library table. The library file is created.
     */
    bool CreateLibrary( const wxString& aFilePath, SYMBOL_LIB_TABLE* aTable )
    {
        return addLibrary( aFilePath, true, aTable );
    }

    /**
     * Add an existing library. The library is added to the library table as well.
     */
    bool AddLibrary( const wxString& aFilePath, SYMBOL_LIB_TABLE* aTable )
    {
        return addLibrary( aFilePath, false, aTable );
    }

    /**
     * Update the part buffer with a new version of the part.
     * The library buffer creates a copy of the part.
     * It is required to save the library to use the updated part in the schematic editor.
     */
    bool UpdatePart( LIB_PART* aPart, const wxString& aLibrary );

    /**
     * Update the part buffer with a new version of the part when the name has changed.
     * The old library buffer will be deleted and a new one created with the new name.
     */
    bool UpdatePartAfterRename( LIB_PART* aPart, const wxString& oldAlias,
                                const wxString& aLibrary );

    /**
     * Remove the part from the part buffer.
     * It is required to save the library to have the part removed in the schematic editor.
     */
    bool RemovePart( const wxString& aName, const wxString& aLibrary );

    /**
     * Return either an alias of a working LIB_PART copy, or alias of the original part if there
     * is no working copy.
     */
    LIB_PART* GetAlias( const wxString& aAlias, const wxString& aLibrary ) const;

    /**
     * Return the part copy from the buffer. In case it does not exist yet, the copy is created.
     * #SYMBOL_LIBRARY_MANAGER retains the ownership.
     */
    LIB_PART* GetBufferedPart( const wxString& aAlias, const wxString& aLibrary );

    /**
     * Return the screen used to edit a specific part. #SYMBOL_LIBRARY_MANAGER retains the
     * ownership.
     */
    SCH_SCREEN* GetScreen( const wxString& aAlias, const wxString& aLibrary );

    /**
     * Return true if part with a specific alias exists in library (either original one or
     * buffered).
     */
    bool PartExists( const wxString& aAlias, const wxString& aLibrary ) const;

    /**
     * Return true if library exists.  If \a aCheckEnabled is set, then the library must
     * also be enabled in the library table.
     */
    bool LibraryExists( const wxString& aLibrary, bool aCheckEnabled = false ) const;

    /**
     * Return true if the library was successfully loaded.
     */
    bool IsLibraryLoaded( const wxString& aLibrary ) const;

    /**
     * Return true if library has unsaved modifications.
     */
    bool IsLibraryModified( const wxString& aLibrary ) const;

    /**
     * Return true if part has unsaved modifications.
     */
    bool IsPartModified( const wxString& aAlias, const wxString& aLibrary ) const;

    /**
     * Clear the modified flag for all parts in a library.
     */
    bool ClearLibraryModified( const wxString& aLibrary ) const;

    /**
     * Clear the modified flag for a part.
     */
    bool ClearPartModified( const wxString& aAlias, const wxString& aLibrary ) const;

    /**
     * Return true if the library is stored in a read-only file.
     *
     * @return True on success, false otherwise.
     */
    bool IsLibraryReadOnly( const wxString& aLibrary ) const;

    /**
     * Save part changes to the library copy used by the schematic editor. Not it is not
     * necessarily saved to the file.
     *
     * @return True on success, false otherwise.
     */
    bool FlushPart( const wxString& aAlias, const wxString& aLibrary );

    /**
     * Save library to a file, including unsaved changes.
     *
     * @param aLibrary is the library name.
     * @param aFileName is the target file name.
     * @return True on success, false otherwise.
     */
    bool SaveLibrary( const wxString& aLibrary, const wxString& aFileName,
                      SCH_IO_MGR::SCH_FILE_T aFileType = SCH_IO_MGR::SCH_FILE_T::SCH_LEGACY );

    /**
     * Revert unsaved changes for a particular part.
     *
     * @return The LIB_ID of the reverted part (which may be different in the case
     * of a rename)
     */
    LIB_ID RevertPart( const wxString& aAlias, const wxString& aLibrary );

    /**
     * Revert unsaved changes for a particular library.
     *
     * @return True on success, false otherwise.
     */
    bool RevertLibrary( const wxString& aLibrary );

    /**
     * Revert all pending changes.
     *
     * @return True if all changes successfully reverted.
     */
    bool RevertAll();

    /**
     * Return a library name that is not currently in use.
     * Used for generating names for new libraries.
     */
    wxString GetUniqueLibraryName() const;

    /**
     * Return the adapter object that provides the stored data.
     */
    wxObjectDataPtr<LIB_TREE_MODEL_ADAPTER>& GetAdapter() { return m_adapter; }

    void GetRootSymbolNames( const wxString& aLibName, wxArrayString& aRootSymbolNames );

    /**
     * Check if symbol \a aSymbolName in library \a aLibraryName is a root symbol that
     * has derived symbols.
     *
     * @return true if \aSymbolName in \a aLibraryName has derived symbols.
     */
    bool HasDerivedSymbols( const wxString& aSymbolName, const wxString& aLibraryName );

private:
    ///< Extract library name basing on the file name.
    static wxString getLibraryName( const wxString& aFilePath );

    ///< Helper function to add either existing or create new library
    bool addLibrary( const wxString& aFilePath, bool aCreate, SYMBOL_LIB_TABLE* aTable );

    ///< Return the current Symbol Library Table.
    SYMBOL_LIB_TABLE* symTable() const;

    SYMBOL_TREE_SYNCHRONIZING_ADAPTER* getAdapter()
    {
        return static_cast<SYMBOL_TREE_SYNCHRONIZING_ADAPTER*>( m_adapter.get() );
    }

    ///< Class to store a working copy of a LIB_PART object and editor context.
    class PART_BUFFER
    {
    public:
        PART_BUFFER( LIB_PART* aPart = nullptr, std::unique_ptr<SCH_SCREEN> aScreen = nullptr );
        ~PART_BUFFER();

        LIB_PART* GetPart() const { return m_part; }
        void SetPart( LIB_PART* aPart );

        LIB_PART* GetOriginal() const { return m_original; }
        void SetOriginal( LIB_PART* aPart );

        bool IsModified() const;
        SCH_SCREEN* GetScreen() const { return m_screen.get(); }

        ///< Transfer the screen ownership
        std::unique_ptr<SCH_SCREEN> RemoveScreen()
        {
            return std::move( m_screen );
        }

        bool SetScreen( std::unique_ptr<SCH_SCREEN> aScreen )
        {
            bool ret = !!m_screen;
            m_screen = std::move( aScreen );
            return ret;
        }

        typedef std::shared_ptr<PART_BUFFER> PTR;
        typedef std::weak_ptr<PART_BUFFER> WEAK_PTR;

    private:
        std::unique_ptr<SCH_SCREEN> m_screen;

        LIB_PART* m_part;        // Working copy
        LIB_PART* m_original;    // Initial state of the part
    };


    ///< Store a working copy of a library.
    class LIB_BUFFER
    {
    public:
        LIB_BUFFER( const wxString& aLibrary ) :
                m_libName( aLibrary ),
                m_hash( 1 )
        { }

        bool IsModified() const
        {
            if( !m_deleted.empty() )
                return true;

            for( const auto& partBuf : m_parts )
            {
                if( partBuf->IsModified() )
                    return true;
            }

            return false;
        }

        int GetHash() const { return m_hash; }

        ///< Return the working copy of a LIB_PART root object with specified alias.
        LIB_PART* GetPart( const wxString& aAlias ) const;

        ///< Create a new buffer to store a part. LIB_BUFFER takes ownership of aCopy.
        bool CreateBuffer( LIB_PART* aCopy, SCH_SCREEN* aScreen );

        ///< Update the buffered part with the contents of \a aCopy.
        bool UpdateBuffer( PART_BUFFER::PTR aPartBuf, LIB_PART* aCopy );

        bool DeleteBuffer( PART_BUFFER::PTR aPartBuf );

        void ClearDeletedBuffer()
        {
            m_deleted.clear();
        }

        ///< Save stored modifications to Symbol Lib Table. It may result in saving the symbol
        ///< to disk as well, depending on the row properties.
        bool SaveBuffer( PART_BUFFER::PTR aPartBuf, SYMBOL_LIB_TABLE* aLibTable );

        ///< Save stored modifications using a plugin. aBuffer decides whether the changes
        ///< should be cached or stored directly to the disk (for SCH_LEGACY_PLUGIN).
        bool SaveBuffer( PART_BUFFER::PTR aPartBuf, SCH_PLUGIN* aPlugin, bool aBuffer );

        ///< Return a part buffer with LIB_PART holding a particular alias
        PART_BUFFER::PTR GetBuffer( const wxString& aAlias ) const;

        ///< Return all buffered parts
        const std::deque<PART_BUFFER::PTR>& GetBuffers() const { return m_parts; }

        /**
         * Check to see any parts in the buffer are derived from a parent named \a aParentName.
         *
         * @param aParentName is the name of the parent to test.
         * @return true if any symbols are found derived from a symbol named \a aParent, otherwise
         *         false.
         */
        bool HasDerivedSymbols( const wxString& aParentName ) const;

        /**
         * Fetch a list of root symbols names from the library buffer.
         *
         * @param aRootSymbolNames is a reference to a list to populate with root symbol names.
         */
        void GetRootSymbolNames( wxArrayString& aRootSymbolNames );

        /**
         * Fetch all of the symbols derived from a \a aSymbolName into \a aList.
         *
         * @param aSymbolName is the name of the symbol to search for derived parts in this
         *                    buffer.
         * @param aList is the list of symbols names derived from \a aSymbolName.
         * @return a size_t count of the number of symbols derived from \a aSymbolName.
         */
        size_t GetDerivedSymbolNames( const wxString& aSymbolName, wxArrayString& aList );

    private:
        /**
         * Remove all symbols derived from \a aParent from the library buffer.
         *
         * @param aParent is the #PART_BUFFER to check against.
         * @return the count of #PART_BUFFER objects removed from the library.
         */
        int removeChildSymbols( PART_BUFFER::PTR aPartBuf );

        std::deque<PART_BUFFER::PTR> m_parts;
        std::deque<PART_BUFFER::PTR> m_deleted;  // Buffer for deleted parts until library is saved
        const wxString               m_libName;  // Buffered library name
        int                          m_hash;
    };

    /**
     * Return a set of #LIB_PART objects belonging to the original library.
     */
    std::set<LIB_PART*> getOriginalParts( const wxString& aLibrary );

    /**
     * Return an existing library buffer or creates one to using Symbol Library Table to get
     * the original data.
     */
    LIB_BUFFER& getLibraryBuffer( const wxString& aLibrary );

    ///< The library buffers
    std::map<wxString, LIB_BUFFER> m_libs;

    SYMBOL_EDIT_FRAME& m_frame;        ///< Parent frame
    LIB_LOGGER         m_logger;
    int                m_syncHash;     ///< Symbol lib table hash value from last synchronization

    wxObjectDataPtr<LIB_TREE_MODEL_ADAPTER> m_adapter;
};

#endif /* SYMBOL_LIBRARY_MANAGER_H */
