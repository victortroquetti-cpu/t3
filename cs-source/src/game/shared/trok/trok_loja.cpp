//=============================================================================
// Trok: dados da loja (scripts/trok_loja.txt). Ver trok_loja.h e design/economia.md.
//=============================================================================

#include "cbase.h"
#include "trok_loja.h"
#include "filesystem.h"
#include "KeyValues.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

static CTrokLojaDados s_TrokLoja;

CTrokLojaDados &TrokLoja( void )
{
	if ( !s_TrokLoja.IsCarregada() )
		s_TrokLoja.Carregar();
	return s_TrokLoja;
}

static int TipoDoTexto( const char *psz )
{
	if ( !V_stricmp( psz, "colete" ) )
		return LOJA_COLETE;
	if ( !V_stricmp( psz, "capacete" ) )
		return LOJA_CAPACETE;
	if ( !V_stricmp( psz, "municao" ) )
		return LOJA_MUNICAO;
	if ( !V_stricmp( psz, "medico" ) )
		return LOJA_MEDICO;
	return LOJA_ARMA;
}

void CTrokLojaDados::Carregar( void )
{
	m_bCarregada = true;
	m_Categorias.RemoveAll();
	m_Itens.RemoveAll();
	m_Classes.RemoveAll();
	m_Abate.RemoveAll();
	m_iAbatePadrao = 300;

	KeyValues *pRoot = new KeyValues( "Loja" );
	if ( !pRoot->LoadFromFile( g_pFullFileSystem, TROK_LOJA_ARQUIVO, "MOD" ) )
	{
		Warning( "Trok: não achei %s; loja vazia.\n", TROK_LOJA_ARQUIVO );
		pRoot->deleteThis();
		return;
	}

	if ( KeyValues *pCat = pRoot->FindKey( "Categorias" ) )
	{
		for ( KeyValues *k = pCat->GetFirstSubKey(); k; k = k->GetNextKey() )
		{
			TrokLojaCategoria_t &c = m_Categorias[ m_Categorias.AddToTail() ];
			V_strncpy( c.szId, k->GetName(), sizeof( c.szId ) );
			V_strncpy( c.szNome, k->GetString(), sizeof( c.szNome ) );
		}
	}

	if ( KeyValues *pCls = pRoot->FindKey( "Classes" ) )
	{
		for ( KeyValues *k = pCls->GetFirstSubKey(); k; k = k->GetNextKey() )
		{
			Par_t &p = m_Classes[ m_Classes.AddToTail() ];
			V_strncpy( p.szChave, k->GetName(), sizeof( p.szChave ) );
			V_strncpy( p.szValor, k->GetString(), sizeof( p.szValor ) );
			p.iValor = 0;
		}
	}

	if ( KeyValues *pAb = pRoot->FindKey( "Abate" ) )
	{
		for ( KeyValues *k = pAb->GetFirstSubKey(); k; k = k->GetNextKey() )
		{
			if ( !V_stricmp( k->GetName(), "padrao" ) )
			{
				m_iAbatePadrao = k->GetInt();
				continue;
			}
			Par_t &p = m_Abate[ m_Abate.AddToTail() ];
			V_strncpy( p.szChave, k->GetName(), sizeof( p.szChave ) );
			p.szValor[ 0 ] = 0;
			p.iValor = k->GetInt();
		}
	}

	if ( KeyValues *pItens = pRoot->FindKey( "Itens" ) )
	{
		for ( KeyValues *k = pItens->GetFirstSubKey(); k; k = k->GetNextKey() )
		{
			TrokLojaItem_t &it = m_Itens[ m_Itens.AddToTail() ];
			V_memset( &it, 0, sizeof( it ) );
			V_strncpy( it.szId, k->GetName(), sizeof( it.szId ) );
			V_strncpy( it.szNome, k->GetString( "nome", k->GetName() ), sizeof( it.szNome ) );
			V_strncpy( it.szCategoria, k->GetString( "categoria", "" ), sizeof( it.szCategoria ) );
			V_strncpy( it.szDescricao, k->GetString( "descricao", "" ), sizeof( it.szDescricao ) );
			V_strncpy( it.szEntidade, k->GetString( "entidade", "" ), sizeof( it.szEntidade ) );
			it.iTipo = TipoDoTexto( k->GetString( "tipo", "arma" ) );
			it.iPreco = MAX( 0, k->GetInt( "preco", 0 ) );
			it.iTime = k->GetInt( "time", 0 );
			it.iMunicao = k->GetInt( "municao", -1 );
			it.iMax = MAX( 0, k->GetInt( "max", 0 ) );
			it.iSlot = k->GetInt( "slot", -1 );
			it.iNivel = k->GetInt( "nivel", 1 );
			it.iArmadura = MAX( 0, k->GetInt( "armadura", 0 ) );
			it.flFolego = MAX( 0.1f, k->GetFloat( "folego", 1.0f ) );
			it.flVelocidade = MAX( 0.1f, k->GetFloat( "velocidade", 1.0f ) );
			it.flProtecao = clamp( k->GetFloat( "protecao", 0.0f ), 0.0f, 1.0f );
			it.iGolpes = MAX( 0, k->GetInt( "golpes", 0 ) );
			it.iVida = MAX( 0, k->GetInt( "vida", 0 ) );
		}
	}

	pRoot->deleteThis();
}

int CTrokLojaDados::ItemIndice( const char *pszId ) const
{
	if ( !pszId )
		return -1;
	FOR_EACH_VEC( m_Itens, i )
	{
		if ( !V_stricmp( m_Itens[ i ].szId, pszId ) )
			return i;
	}
	return -1;
}

const TrokLojaItem_t *CTrokLojaDados::Item( const char *pszId ) const
{
	const int i = ItemIndice( pszId );
	return i >= 0 ? &m_Itens[ i ] : NULL;
}

const char *CTrokLojaDados::Classe( const char *pszEntidade ) const
{
	if ( !pszEntidade )
		return "";
	FOR_EACH_VEC( m_Classes, i )
	{
		if ( !V_stricmp( m_Classes[ i ].szChave, pszEntidade ) )
			return m_Classes[ i ].szValor;
	}
	return "";
}

int CTrokLojaDados::GanhoAbate( const char *pszEntidade ) const
{
	const char *pszClasse = Classe( pszEntidade );
	if ( pszClasse[ 0 ] )
	{
		FOR_EACH_VEC( m_Abate, i )
		{
			if ( !V_stricmp( m_Abate[ i ].szChave, pszClasse ) )
				return m_Abate[ i ].iValor;
		}
	}
	return m_iAbatePadrao;
}
