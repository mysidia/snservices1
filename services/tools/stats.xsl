<xsl:stylesheet version="1.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform">

<xsl:template match="stats">
<HTML>
 <BODY>
   <xsl:apply-templates />
 </BODY>
</HTML>
</xsl:template>

<xsl:template match="stats/statsrun">
	<div>
	   <xsl:apply-templates />
	</div>
</xsl:template>

<xsl:template match="stats/statsrun/nickstats">
	<hr /><div>
		UTC: <xsl:value-of select="@time" />
		Total nicks: <xsl:value-of select="@total" /><br />
		Number urls: <xsl:value-of select="@numurls" /><br />
		E-mail (or none) set: <xsl:value-of select="@numemails" /><br />
		Access list entries: <xsl:value-of select="@accitems" /><br />
	</div>
	<xsl:apply-templates select="emails" />

	<div>Banished nicknames:</div>
	<div><xsl:apply-templates select="bannick" /></div>
	<div>Held nicknames:</div>
	<div><xsl:apply-templates select="heldnick" /></div>
</xsl:template>

<xsl:template match="stats/statsrun/nickstats/emails">
	<div>
		Number e-mail addresses: <xsl:value-of select="@valid" /><br />
		Users specifying 'none': <xsl:value-of select="@none" /><br />
	</div>
</xsl:template>

<xsl:template match="stats/statsrun/nickstats/bannick">
	 <xsl:value-of select="." /> <xsl:value-of select="' '" />
</xsl:template>

<xsl:template match="stats/statsrun/nickstats/heldnick">
	<xsl:value-of select="." /> <xsl:value-of select="' '" />

</xsl:template>


<xsl:template match="stats/statsrun/chanstats">
Held channels:
	<div>
		<xsl:apply-templates select="heldchan" />
	</div>
Banished channels:
	<div>
		<xsl:apply-templates select="banchan" />
	</div>
</xsl:template>

<xsl:template match="stats/statsrun/chanstats/heldchan">
	<xsl:value-of select="." /><xsl:value-of select="' '" />
</xsl:template>

<xsl:template match="stats/statsrun/chanstats/banchan">
	<xsl:value-of select="." /><xsl:value-of select="' '" />

</xsl:template>


</xsl:stylesheet>
